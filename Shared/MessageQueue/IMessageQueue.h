#pragma once

#include "MessageTypes.h"
#include <vector>
#include <string>
#include <future>

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
        virtual QueueResult GetQueueInfo(const std::string& QueueName, QueueConfig& OutConfig) const = 0;
        virtual QueueResult UpdateQueueConfig(const std::string& QueueName, const QueueConfig& Config) = 0;

        // 主题管理（发布订阅）
        virtual QueueResult CreateTopic(const TopicConfig& Config) = 0;
        virtual QueueResult DeleteTopic(const std::string& TopicName) = 0;
        virtual bool TopicExists(const std::string& TopicName) const = 0;
        virtual std::vector<std::string> ListTopics() const = 0;
        virtual QueueResult GetTopicInfo(const std::string& TopicName, TopicConfig& OutConfig) const = 0;

        // 消息发送（生产者）
        virtual QueueResult SendMessage(const std::string& QueueName, MessagePtr Message) = 0;
        virtual QueueResult SendMessageAsync(const std::string& QueueName, MessagePtr Message, AcknowledgeHandler Handler = nullptr) = 0;
        virtual QueueResult SendBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages) = 0;
        virtual std::future<QueueResult> SendMessageFuture(const std::string& QueueName, MessagePtr Message) = 0;

        // 消息接收（消费者）
        virtual QueueResult ReceiveMessage(const std::string& QueueName, MessagePtr& OutMessage, uint32_t TimeoutMs = 0) = 0;
        virtual QueueResult ReceiveBatchMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages, uint32_t MaxCount = 10, uint32_t TimeoutMs = 1000) = 0;
        virtual QueueResult PeekMessage(const std::string& QueueName, MessagePtr& OutMessage) = 0;

        // 消息确认
        virtual QueueResult AcknowledgeMessage(const std::string& QueueName, MessageId MessageId) = 0;
        virtual QueueResult RejectMessage(const std::string& QueueName, MessageId MessageId, bool Requeue = true) = 0;
        virtual QueueResult AcknowledgeBatch(const std::string& QueueName, const std::vector<MessageId>& MessageIds) = 0;

        // 消费者管理
        virtual QueueResult RegisterConsumer(const std::string& QueueName, const ConsumerConfig& Config, MessageHandler Handler) = 0;
        virtual QueueResult RegisterBatchConsumer(const std::string& QueueName, const ConsumerConfig& Config, BatchMessageHandler Handler) = 0;
        virtual QueueResult UnregisterConsumer(const std::string& QueueName, const std::string& ConsumerId) = 0;
        virtual std::vector<std::string> GetActiveConsumers(const std::string& QueueName) const = 0;

        // 生产者管理
        virtual QueueResult RegisterProducer(const std::string& QueueName, const ProducerConfig& Config) = 0;
        virtual QueueResult UnregisterProducer(const std::string& QueueName, const std::string& ProducerId) = 0;
        virtual std::vector<std::string> GetActiveProducers(const std::string& QueueName) const = 0;

        // 发布订阅
        virtual QueueResult PublishMessage(const std::string& TopicName, MessagePtr Message) = 0;
        virtual QueueResult PublishBatchMessages(const std::string& TopicName, const std::vector<MessagePtr>& Messages) = 0;
        virtual QueueResult Subscribe(const std::string& TopicName, const std::string& SubscriberId, MessageHandler Handler) = 0;
        virtual QueueResult Unsubscribe(const std::string& TopicName, const std::string& SubscriberId) = 0;
        virtual std::vector<std::string> GetActiveSubscribers(const std::string& TopicName) const = 0;

        // 广播消息
        virtual QueueResult BroadcastMessage(MessagePtr Message) = 0;
        virtual QueueResult BroadcastToQueues(const std::vector<std::string>& QueueNames, MessagePtr Message) = 0;
        virtual QueueResult BroadcastToTopics(const std::vector<std::string>& TopicNames, MessagePtr Message) = 0;

        // 延迟和定时消息
        virtual QueueResult ScheduleMessage(const std::string& QueueName, MessagePtr Message, uint32_t DelayMs) = 0;
        virtual QueueResult ScheduleRecurringMessage(const std::string& QueueName, MessagePtr Message, uint32_t IntervalMs, uint32_t Count = 0) = 0;
        virtual QueueResult CancelScheduledMessage(MessageId MessageId) = 0;

        // 消息过滤和路由
        virtual QueueResult SetMessageFilter(const std::string& QueueName, const std::string& FilterExpression) = 0;
        virtual QueueResult SetMessageRouter(const std::string& SourceQueue, const std::string& TargetQueue, const std::string& RoutingKey) = 0;
        virtual QueueResult RemoveMessageFilter(const std::string& QueueName) = 0;
        virtual QueueResult RemoveMessageRouter(const std::string& SourceQueue, const std::string& TargetQueue) = 0;

        // 死信队列管理
        virtual QueueResult GetDeadLetterMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages, uint32_t MaxCount = 100) = 0;
        virtual QueueResult RequeueDeadLetterMessage(const std::string& QueueName, MessageId MessageId) = 0;
        virtual QueueResult PurgeDeadLetterQueue(const std::string& QueueName) = 0;

        // 统计和监控
        virtual QueueResult GetQueueStats(const std::string& QueueName, QueueStats& OutStats) const = 0;
        virtual QueueResult GetTopicStats(const std::string& TopicName, QueueStats& OutStats) const = 0;
        virtual QueueResult GetGlobalStats(QueueStats& OutStats) const = 0;
        virtual std::vector<MessagePtr> GetPendingMessages(const std::string& QueueName, uint32_t MaxCount = 100) const = 0;

        // 持久化管理
        virtual QueueResult SaveToDisk() = 0;
        virtual QueueResult LoadFromDisk() = 0;
        virtual QueueResult EnablePersistence(const std::string& QueueName, PersistenceMode Mode) = 0;
        virtual QueueResult DisablePersistence(const std::string& QueueName) = 0;

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
        virtual QueueResult SendMessageAsync(const std::string& QueueName, MessagePtr Message, AcknowledgeHandler Handler = nullptr) = 0;
        virtual QueueResult SendBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages) = 0;

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

        virtual QueueResult Initialize(const std::string& TopicName, const std::string& SubscriberId) = 0;
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

} // namespace Helianthus::MessageQueue