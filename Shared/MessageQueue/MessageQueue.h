#pragma once

#include "IMessageQueue.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <functional>

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
        QueueResult SendMessageAsync(const std::string& QueueName, MessagePtr Message, AcknowledgeHandler Handler = nullptr) override;
        QueueResult SendBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages) override;
        std::future<QueueResult> SendMessageFuture(const std::string& QueueName, MessagePtr Message) override;

        // 消息接收（消费者）
        QueueResult ReceiveMessage(const std::string& QueueName, MessagePtr& OutMessage, uint32_t TimeoutMs = 0) override;
        QueueResult ReceiveBatchMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages, uint32_t MaxCount = 10, uint32_t TimeoutMs = 1000) override;
        QueueResult PeekMessage(const std::string& QueueName, MessagePtr& OutMessage) override;

        // 消息确认
        QueueResult AcknowledgeMessage(const std::string& QueueName, MessageId MessageId) override;
        QueueResult RejectMessage(const std::string& QueueName, MessageId MessageId, bool Requeue = true) override;
        QueueResult AcknowledgeBatch(const std::string& QueueName, const std::vector<MessageId>& MessageIds) override;

        // 消费者管理
        QueueResult RegisterConsumer(const std::string& QueueName, const ConsumerConfig& Config, MessageHandler Handler) override;
        QueueResult RegisterBatchConsumer(const std::string& QueueName, const ConsumerConfig& Config, BatchMessageHandler Handler) override;
        QueueResult UnregisterConsumer(const std::string& QueueName, const std::string& ConsumerId) override;
        std::vector<std::string> GetActiveConsumers(const std::string& QueueName) const override;

        // 生产者管理
        QueueResult RegisterProducer(const std::string& QueueName, const ProducerConfig& Config) override;
        QueueResult UnregisterProducer(const std::string& QueueName, const std::string& ProducerId) override;
        std::vector<std::string> GetActiveProducers(const std::string& QueueName) const override;

        // 发布订阅
        QueueResult PublishMessage(const std::string& TopicName, MessagePtr Message) override;
        QueueResult PublishBatchMessages(const std::string& TopicName, const std::vector<MessagePtr>& Messages) override;
        QueueResult Subscribe(const std::string& TopicName, const std::string& SubscriberId, MessageHandler Handler) override;
        QueueResult Unsubscribe(const std::string& TopicName, const std::string& SubscriberId) override;
        std::vector<std::string> GetActiveSubscribers(const std::string& TopicName) const override;

        // 广播消息
        QueueResult BroadcastMessage(MessagePtr Message) override;
        QueueResult BroadcastToQueues(const std::vector<std::string>& QueueNames, MessagePtr Message) override;
        QueueResult BroadcastToTopics(const std::vector<std::string>& TopicNames, MessagePtr Message) override;

        // 延迟和定时消息
        QueueResult ScheduleMessage(const std::string& QueueName, MessagePtr Message, uint32_t DelayMs) override;
        QueueResult ScheduleRecurringMessage(const std::string& QueueName, MessagePtr Message, uint32_t IntervalMs, uint32_t Count = 0) override;
        QueueResult CancelScheduledMessage(MessageId MessageId) override;

        // 消息过滤和路由
        QueueResult SetMessageFilter(const std::string& QueueName, const std::string& FilterExpression) override;
        QueueResult SetMessageRouter(const std::string& SourceQueue, const std::string& TargetQueue, const std::string& RoutingKey) override;
        QueueResult RemoveMessageFilter(const std::string& QueueName) override;
        QueueResult RemoveMessageRouter(const std::string& SourceQueue, const std::string& TargetQueue) override;

        // 死信队列管理
        QueueResult GetDeadLetterMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages, uint32_t MaxCount = 100) override;
        QueueResult RequeueDeadLetterMessage(const std::string& QueueName, MessageId MessageId) override;
        QueueResult PurgeDeadLetterQueue(const std::string& QueueName) override;

        // 统计和监控
        QueueResult GetQueueStats(const std::string& QueueName, QueueStats& OutStats) const override;
        QueueResult GetTopicStats(const std::string& TopicName, QueueStats& OutStats) const override;
        QueueResult GetGlobalStats(QueueStats& OutStats) const override;
        std::vector<MessagePtr> GetPendingMessages(const std::string& QueueName, uint32_t MaxCount = 100) const override;

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

    private:
        // 内部数据结构
        struct QueueData
        {
            QueueConfig Config;
            std::queue<MessagePtr> Messages;
            std::priority_queue<MessagePtr, std::vector<MessagePtr>, std::function<bool(const MessagePtr&, const MessagePtr&)>> PriorityMessages;
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
            
            ScheduledMessage(MessagePtr Msg, const std::string& Queue, MessageTimestamp ExecTime, 
                           uint32_t Interval = 0, uint32_t Count = 0);
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
        
        // 全局配置
        std::unordered_map<std::string, std::string> GlobalConfig;
        mutable std::shared_mutex ConfigMutex;
        
        // 统计信息
        QueueStats GlobalStats;
        mutable std::shared_mutex StatsMutex;
        
        // 内部方法
        MessageId GenerateMessageId();
        QueueData* GetQueueData(const std::string& QueueName);
        const QueueData* GetQueueData(const std::string& QueueName) const;
        TopicData* GetTopicData(const std::string& TopicName);
        const TopicData* GetTopicData(const std::string& TopicName) const;
        
        void ProcessConsumerQueue(const std::string& QueueName);
        void ProcessScheduledMessages();
        void NotifyEvent(const std::string& QueueName, const std::string& Event, const std::string& Details);
        void NotifyError(QueueResult Result, const std::string& ErrorMessage);
        
        bool ValidateMessage(MessagePtr Message);
        bool ValidateQueueConfig(const QueueConfig& Config);
        bool ValidateTopicConfig(const TopicConfig& Config);
        bool IsMessageExpired(MessagePtr Message);
        void MoveToDeadLetter(const std::string& QueueName, MessagePtr Message);
        
        QueueResult DeliverMessageToConsumers(const std::string& QueueName, MessagePtr Message);
        QueueResult DeliverMessageToSubscribers(const std::string& TopicName, MessagePtr Message);
        
        void UpdateQueueStats(const std::string& QueueName, bool MessageSent, bool MessageReceived = false);
        void UpdateTopicStats(const std::string& TopicName, bool MessagePublished);
        void UpdateGlobalStats(bool MessageProcessed, bool MessageFailed = false);
        
        std::string GetDeadLetterQueueName(const std::string& QueueName);
        QueueResult EnsureDeadLetterQueue(const std::string& QueueName);
        
        bool EvaluateMessageFilter(const std::string& Filter, MessagePtr Message);
        QueueResult RouteMessage(const std::string& SourceQueue, MessagePtr Message);
    };

} // namespace Helianthus::MessageQueue