#pragma once

#include <future>
#include <string>
#include <vector>

#include "MessageTypes.h"
#include "MessagePersistence.h"

namespace Helianthus::MessageQueue
{
/**
 * @brief 消息队列核心接口
 *
 * 提供高性能的消息队列功能，支持多种消息传递模式：
 * - 点对点队列
 * - 发布订阅主题
 * - 广播消息
 * - 优先级队列
 * - 延迟队列
 */
class IMessageQueue
{
public:
    virtual ~IMessageQueue() = default;

    // 生命周期管理
    virtual QueueResult Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // 队列管理
    virtual QueueResult CreateQueue(const QueueConfig& Config) = 0;
    virtual QueueResult DeleteQueue(const std::string& QueueName) = 0;
    virtual QueueResult PurgeQueue(const std::string& QueueName) = 0;
    virtual bool QueueExists(const std::string& QueueName) const = 0;
    virtual std::vector<std::string> ListQueues() const = 0;
    virtual QueueResult GetQueueInfo(const std::string& QueueName,
                                     QueueConfig& OutConfig) const = 0;
    virtual QueueResult UpdateQueueConfig(const std::string& QueueName,
                                          const QueueConfig& Config) = 0;

    // 主题管理（发布订阅）
    virtual QueueResult CreateTopic(const TopicConfig& Config) = 0;
    virtual QueueResult DeleteTopic(const std::string& TopicName) = 0;
    virtual bool TopicExists(const std::string& TopicName) const = 0;
    virtual std::vector<std::string> ListTopics() const = 0;
    virtual QueueResult GetTopicInfo(const std::string& TopicName,
                                     TopicConfig& OutConfig) const = 0;

    // 消息发送（生产者）
    virtual QueueResult SendMessage(const std::string& QueueName, MessagePtr Message) = 0;
    virtual QueueResult SendMessageAsync(const std::string& QueueName,
                                         MessagePtr Message,
                                         AcknowledgeHandler Handler = nullptr) = 0;
    virtual QueueResult SendBatchMessages(const std::string& QueueName,
                                          const std::vector<MessagePtr>& Messages) = 0;
    virtual std::future<QueueResult> SendMessageFuture(const std::string& QueueName,
                                                       MessagePtr Message) = 0;

    // 消息接收（消费者）
    virtual QueueResult ReceiveMessage(const std::string& QueueName,
                                       MessagePtr& OutMessage,
                                       uint32_t TimeoutMs = 0) = 0;
    virtual QueueResult ReceiveBatchMessages(const std::string& QueueName,
                                             std::vector<MessagePtr>& OutMessages,
                                             uint32_t MaxCount = 10,
                                             uint32_t TimeoutMs = 1000) = 0;
    virtual QueueResult PeekMessage(const std::string& QueueName, MessagePtr& OutMessage) = 0;

    // 消息确认
    virtual QueueResult AcknowledgeMessage(const std::string& QueueName, MessageId MessageId) = 0;
    virtual QueueResult
    RejectMessage(const std::string& QueueName, MessageId MessageId, bool Requeue = true) = 0;
    virtual QueueResult AcknowledgeBatch(const std::string& QueueName,
                                         const std::vector<MessageId>& MessageIds) = 0;

    // 消费者管理
    virtual QueueResult RegisterConsumer(const std::string& QueueName,
                                         const ConsumerConfig& Config,
                                         MessageHandler Handler) = 0;
    virtual QueueResult RegisterBatchConsumer(const std::string& QueueName,
                                              const ConsumerConfig& Config,
                                              BatchMessageHandler Handler) = 0;
    virtual QueueResult UnregisterConsumer(const std::string& QueueName,
                                           const std::string& ConsumerId) = 0;
    virtual std::vector<std::string> GetActiveConsumers(const std::string& QueueName) const = 0;

    // 生产者管理
    virtual QueueResult RegisterProducer(const std::string& QueueName,
                                         const ProducerConfig& Config) = 0;
    virtual QueueResult UnregisterProducer(const std::string& QueueName,
                                           const std::string& ProducerId) = 0;
    virtual std::vector<std::string> GetActiveProducers(const std::string& QueueName) const = 0;

    // 发布订阅
    virtual QueueResult PublishMessage(const std::string& TopicName, MessagePtr Message) = 0;
    virtual QueueResult PublishBatchMessages(const std::string& TopicName,
                                             const std::vector<MessagePtr>& Messages) = 0;
    virtual QueueResult Subscribe(const std::string& TopicName,
                                  const std::string& SubscriberId,
                                  MessageHandler Handler) = 0;
    virtual QueueResult Unsubscribe(const std::string& TopicName,
                                    const std::string& SubscriberId) = 0;
    virtual std::vector<std::string> GetActiveSubscribers(const std::string& TopicName) const = 0;

    // 广播消息
    virtual QueueResult BroadcastMessage(MessagePtr Message) = 0;
    virtual QueueResult BroadcastToQueues(const std::vector<std::string>& QueueNames,
                                          MessagePtr Message) = 0;
    virtual QueueResult BroadcastToTopics(const std::vector<std::string>& TopicNames,
                                          MessagePtr Message) = 0;

    // 延迟和定时消息
    virtual QueueResult
    ScheduleMessage(const std::string& QueueName, MessagePtr Message, uint32_t DelayMs) = 0;
    virtual QueueResult ScheduleRecurringMessage(const std::string& QueueName,
                                                 MessagePtr Message,
                                                 uint32_t IntervalMs,
                                                 uint32_t Count = 0) = 0;
    virtual QueueResult CancelScheduledMessage(MessageId MessageId) = 0;

    // 消息过滤和路由
    virtual QueueResult SetMessageFilter(const std::string& QueueName,
                                         const std::string& FilterExpression) = 0;
    virtual QueueResult SetMessageRouter(const std::string& SourceQueue,
                                         const std::string& TargetQueue,
                                         const std::string& RoutingKey) = 0;
    virtual QueueResult RemoveMessageFilter(const std::string& QueueName) = 0;
    virtual QueueResult RemoveMessageRouter(const std::string& SourceQueue,
                                            const std::string& TargetQueue) = 0;

    // 死信队列管理
    virtual QueueResult GetDeadLetterMessages(const std::string& QueueName,
                                              std::vector<MessagePtr>& OutMessages,
                                              uint32_t MaxCount = 100) = 0;
    virtual QueueResult RequeueDeadLetterMessage(const std::string& QueueName,
                                                 MessageId MessageId) = 0;
    virtual QueueResult PurgeDeadLetterQueue(const std::string& QueueName) = 0;

    // 统计和监控
    virtual QueueResult GetQueueStats(const std::string& QueueName, QueueStats& OutStats) const = 0;
    virtual QueueResult GetTopicStats(const std::string& TopicName, QueueStats& OutStats) const = 0;
    virtual QueueResult GetGlobalStats(QueueStats& OutStats) const = 0;
    virtual std::vector<MessagePtr> GetPendingMessages(const std::string& QueueName,
                                                       uint32_t MaxCount = 100) const = 0;

    // DLQ监控
    virtual QueueResult GetDeadLetterQueueStats(const std::string& QueueName, 
                                                DeadLetterQueueStats& OutStats) const = 0;
    virtual QueueResult GetAllDeadLetterQueueStats(std::vector<DeadLetterQueueStats>& OutStats) const = 0;
    virtual QueueResult SetDeadLetterAlertConfig(const std::string& QueueName, 
                                                 const DeadLetterAlertConfig& Config) = 0;
    virtual QueueResult GetDeadLetterAlertConfig(const std::string& QueueName, 
                                                 DeadLetterAlertConfig& OutConfig) const = 0;
    virtual QueueResult GetActiveDeadLetterAlerts(const std::string& QueueName,
                                                  std::vector<DeadLetterAlert>& OutAlerts) const = 0;
    virtual QueueResult GetAllActiveDeadLetterAlerts(std::vector<DeadLetterAlert>& OutAlerts) const = 0;
    virtual QueueResult ClearDeadLetterAlert(const std::string& QueueName, 
                                             DeadLetterAlertType AlertType) = 0;
    virtual QueueResult ClearAllDeadLetterAlerts(const std::string& QueueName) = 0;
    virtual void SetDeadLetterAlertHandler(DeadLetterAlertHandler Handler) = 0;
    virtual void SetDeadLetterStatsHandler(DeadLetterStatsHandler Handler) = 0;

    // 队列指标
    virtual QueueResult GetQueueMetrics(const std::string& QueueName, QueueMetrics& OutMetrics) const = 0;
    virtual QueueResult GetAllQueueMetrics(std::vector<QueueMetrics>& OutMetrics) const = 0;

    // 持久化管理
    virtual QueueResult SaveToDisk() = 0;
    virtual QueueResult LoadFromDisk() = 0;
    virtual QueueResult EnablePersistence(const std::string& QueueName, PersistenceMode Mode) = 0;
    virtual QueueResult DisablePersistence(const std::string& QueueName) = 0;
    
    // 持久化统计
    virtual IMessagePersistence::PersistenceStats GetPersistenceStats() const = 0;
    virtual void ResetPersistenceStats() = 0;

    // 集群和复制
    virtual QueueResult EnableReplication(const std::vector<std::string>& ReplicaNodes) = 0;
    virtual QueueResult DisableReplication() = 0;
    virtual bool IsReplicationEnabled() const = 0;
    virtual QueueResult SyncWithReplicas() = 0;

    // 事件回调
    virtual void SetQueueEventHandler(QueueEventHandler Handler) = 0;
    virtual void SetErrorHandler(ErrorHandler Handler) = 0;
    virtual void RemoveAllHandlers() = 0;

    // 配置和调优
    virtual QueueResult SetGlobalConfig(const std::string& Key, const std::string& Value) = 0;
    virtual std::string GetGlobalConfig(const std::string& Key) const = 0;
    virtual QueueResult FlushAll() = 0;
    virtual QueueResult CompactQueues() = 0;

    // 调试和诊断
    virtual std::string GetQueueInfo() const = 0;
    virtual std::vector<std::string> GetQueueDiagnostics(const std::string& QueueName) const = 0;
    virtual QueueResult ValidateQueue(const std::string& QueueName) = 0;

    // 集群/分片/副本接口
    virtual QueueResult SetClusterConfig(const ClusterConfig& Config) = 0;
    virtual QueueResult GetClusterConfig(ClusterConfig& OutConfig) const = 0;
    virtual QueueResult GetShardForKey(const std::string& Key, ShardId& OutShardId, std::string& OutNodeId) const = 0;
    virtual QueueResult GetShardReplicas(ShardId Shard, std::vector<ReplicaInfo>& OutReplicas) const = 0;
    virtual QueueResult SetNodeHealth(const std::string& NodeId, bool Healthy) = 0;
    // 导出每分片详细状态（shard_id、leader、followers健康）
    virtual QueueResult GetClusterShardStatuses(std::vector<ShardInfo>& OutShards) const = 0;

    // 基础HA：主选举/降级/查询
    virtual QueueResult PromoteToLeader(ShardId Shard, const std::string& NodeId) = 0;
    virtual QueueResult DemoteToFollower(ShardId Shard, const std::string& NodeId) = 0;
    virtual QueueResult GetCurrentLeader(ShardId Shard, std::string& OutNodeId) const = 0;

    // 事件回调
    virtual void SetLeaderChangeHandler(LeaderChangeHandler Handler) = 0;
    virtual void SetFailoverHandler(FailoverHandler Handler) = 0;

    // 事务管理
    virtual TransactionId BeginTransaction(const std::string& Description = "", uint32_t TimeoutMs = 30000) = 0;
    virtual QueueResult CommitTransaction(TransactionId Id) = 0;
    virtual QueueResult RollbackTransaction(TransactionId Id, const std::string& Reason = "") = 0;
    virtual QueueResult AbortTransaction(TransactionId Id, const std::string& Reason = "") = 0;
    
    // 事务内操作
    virtual QueueResult SendMessageInTransaction(TransactionId Id, const std::string& QueueName, MessagePtr Message) = 0;
    virtual QueueResult AcknowledgeMessageInTransaction(TransactionId Id, const std::string& QueueName, MessageId MessageId) = 0;
    virtual QueueResult RejectMessageInTransaction(TransactionId Id, const std::string& QueueName, MessageId MessageId, const std::string& Reason = "") = 0;
    virtual QueueResult CreateQueueInTransaction(TransactionId Id, const QueueConfig& Config) = 0;
    virtual QueueResult DeleteQueueInTransaction(TransactionId Id, const std::string& QueueName) = 0;
    
    // 事务查询
    virtual QueueResult GetTransactionStatus(TransactionId Id, TransactionStatus& Status) = 0;
    virtual QueueResult GetTransactionInfo(TransactionId Id, Transaction& Info) = 0;
    virtual QueueResult GetTransactionStats(TransactionStats& Stats) = 0;
    
    // 事务回调设置
    virtual void SetTransactionCommitHandler(TransactionCommitHandler Handler) = 0;
    virtual void SetTransactionRollbackHandler(TransactionRollbackHandler Handler) = 0;
    virtual void SetTransactionTimeoutHandler(TransactionTimeoutHandler Handler) = 0;
    
    // 分布式事务支持
    virtual QueueResult BeginDistributedTransaction(const std::string& CoordinatorId, const std::string& Description = "", uint32_t TimeoutMs = 30000) = 0;
    virtual QueueResult PrepareTransaction(TransactionId Id) = 0;
    virtual QueueResult CommitDistributedTransaction(TransactionId Id) = 0;
    virtual QueueResult RollbackDistributedTransaction(TransactionId Id, const std::string& Reason = "") = 0;

    // 压缩和加密管理
    virtual QueueResult SetCompressionConfig(const std::string& QueueName, const CompressionConfig& Config) = 0;
    virtual QueueResult GetCompressionConfig(const std::string& QueueName, CompressionConfig& OutConfig) const = 0;
    virtual QueueResult SetEncryptionConfig(const std::string& QueueName, const EncryptionConfig& Config) = 0;
    virtual QueueResult GetEncryptionConfig(const std::string& QueueName, EncryptionConfig& OutConfig) const = 0;
    
    // 压缩和加密统计
    virtual QueueResult GetCompressionStats(const std::string& QueueName, CompressionStats& OutStats) const = 0;
    virtual QueueResult GetAllCompressionStats(std::vector<CompressionStats>& OutStats) const = 0;
    virtual QueueResult GetEncryptionStats(const std::string& QueueName, EncryptionStats& OutStats) const = 0;
    virtual QueueResult GetAllEncryptionStats(std::vector<EncryptionStats>& OutStats) const = 0;
    
    // 手动压缩和加密
    virtual QueueResult CompressMessage(MessagePtr Message, CompressionAlgorithm Algorithm = CompressionAlgorithm::GZIP) = 0;
    virtual QueueResult DecompressMessage(MessagePtr Message) = 0;
    virtual QueueResult EncryptMessage(MessagePtr Message, EncryptionAlgorithm Algorithm = EncryptionAlgorithm::AES_256_GCM, const EncryptionConfig& Cfg = EncryptionConfig{}) = 0;
    virtual QueueResult DecryptMessage(MessagePtr Message) = 0;

    // 监控告警管理
    virtual QueueResult SetAlertConfig(const AlertConfig& Config) = 0;
    virtual QueueResult GetAlertConfig(AlertType Type, const std::string& QueueName, AlertConfig& OutConfig) const = 0;
    virtual QueueResult GetAllAlertConfigs(std::vector<AlertConfig>& OutConfigs) const = 0;
    virtual QueueResult DeleteAlertConfig(AlertType Type, const std::string& QueueName) = 0;
    
    // 告警查询
    virtual QueueResult GetActiveAlerts(std::vector<Alert>& OutAlerts) const = 0;
    virtual QueueResult GetAlertHistory(uint32_t Limit, std::vector<Alert>& OutAlerts) const = 0;
    virtual QueueResult GetAlertStats(AlertStats& OutStats) const = 0;
    
    // 告警操作
    virtual QueueResult AcknowledgeAlert(uint64_t AlertId) = 0;
    virtual QueueResult ResolveAlert(uint64_t AlertId, const std::string& ResolutionNote = "") = 0;
    virtual QueueResult ClearAllAlerts() = 0;
    
    // 告警回调设置
    virtual void SetAlertHandler(AlertHandler Handler) = 0;
    virtual void SetAlertConfigHandler(AlertConfigHandler Handler) = 0;

    // 性能优化管理
    virtual QueueResult SetMemoryPoolConfig(const MemoryPoolConfig& Config) = 0;
    virtual QueueResult GetMemoryPoolConfig(MemoryPoolConfig& OutConfig) const = 0;
    virtual QueueResult SetBufferConfig(const BufferConfig& Config) = 0;
    virtual QueueResult GetBufferConfig(BufferConfig& OutConfig) const = 0;
    
    // 性能统计
    virtual QueueResult GetPerformanceStats(PerformanceStats& OutStats) const = 0;
    virtual QueueResult ResetPerformanceStats() = 0;
    
    // 内存池管理
    virtual QueueResult AllocateFromPool(size_t Size, void*& OutPtr) = 0;
    virtual QueueResult DeallocateToPool(void* Ptr, size_t Size) = 0;
    virtual QueueResult CompactMemoryPool() = 0;
    
    // 零拷贝操作
    virtual QueueResult CreateZeroCopyBuffer(const void* Data, size_t Size, ZeroCopyBuffer& OutBuffer) = 0;
    virtual QueueResult ReleaseZeroCopyBuffer(ZeroCopyBuffer& Buffer) = 0;
    virtual QueueResult SendMessageZeroCopy(const std::string& QueueName, const ZeroCopyBuffer& Buffer) = 0;
    
    // 批处理操作
    virtual QueueResult CreateBatch(uint32_t& OutBatchId) = 0;
    virtual QueueResult CreateBatchForQueue(const std::string& QueueName, uint32_t& OutBatchId) = 0;
    virtual QueueResult AddToBatch(uint32_t BatchId, MessagePtr Message) = 0;
    virtual QueueResult CommitBatch(uint32_t BatchId) = 0;
    virtual QueueResult AbortBatch(uint32_t BatchId) = 0;
    virtual QueueResult GetBatchInfo(uint32_t BatchId, BatchMessage& OutBatch) const = 0;
};

/**
 * @brief 消息消费者接口
 */
class IMessageConsumer
{
public:
    virtual ~IMessageConsumer() = default;

    virtual QueueResult Initialize(const ConsumerConfig& Config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    virtual QueueResult StartConsuming(const std::string& QueueName, MessageHandler Handler) = 0;
    virtual QueueResult StopConsuming() = 0;
    virtual bool IsConsuming() const = 0;

    virtual QueueResult SetMessageHandler(MessageHandler Handler) = 0;
    virtual QueueResult SetBatchMessageHandler(BatchMessageHandler Handler) = 0;
    virtual QueueResult SetErrorHandler(ErrorHandler Handler) = 0;

    virtual std::string GetConsumerId() const = 0;
    virtual std::string GetQueueName() const = 0;
    virtual uint64_t GetProcessedMessageCount() const = 0;
    virtual uint64_t GetFailedMessageCount() const = 0;
};

/**
 * @brief 消息生产者接口
 */
class IMessageProducer
{
public:
    virtual ~IMessageProducer() = default;

    virtual QueueResult Initialize(const ProducerConfig& Config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    virtual QueueResult SendMessage(const std::string& QueueName, MessagePtr Message) = 0;
    virtual QueueResult SendMessageAsync(const std::string& QueueName,
                                         MessagePtr Message,
                                         AcknowledgeHandler Handler = nullptr) = 0;
    virtual QueueResult SendBatchMessages(const std::string& QueueName,
                                          const std::vector<MessagePtr>& Messages) = 0;

    virtual std::string GetProducerId() const = 0;
    virtual uint64_t GetSentMessageCount() const = 0;
    virtual uint64_t GetFailedMessageCount() const = 0;
};

/**
 * @brief 主题发布者接口
 */
class ITopicPublisher
{
public:
    virtual ~ITopicPublisher() = default;

    virtual QueueResult Initialize(const std::string& TopicName) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    virtual QueueResult PublishMessage(MessagePtr Message) = 0;
    virtual QueueResult PublishBatchMessages(const std::vector<MessagePtr>& Messages) = 0;

    virtual std::string GetTopicName() const = 0;
    virtual uint64_t GetPublishedMessageCount() const = 0;
};

/**
 * @brief 主题订阅者接口
 */
class ITopicSubscriber
{
public:
    virtual ~ITopicSubscriber() = default;

    virtual QueueResult Initialize(const std::string& TopicName,
                                   const std::string& SubscriberId) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    virtual QueueResult StartSubscription(MessageHandler Handler) = 0;
    virtual QueueResult StopSubscription() = 0;
    virtual bool IsSubscribed() const = 0;

    virtual QueueResult SetMessageHandler(MessageHandler Handler) = 0;
    virtual QueueResult SetErrorHandler(ErrorHandler Handler) = 0;

    virtual std::string GetTopicName() const = 0;
    virtual std::string GetSubscriberId() const = 0;
    virtual uint64_t GetReceivedMessageCount() const = 0;
};

}  // namespace Helianthus::MessageQueue