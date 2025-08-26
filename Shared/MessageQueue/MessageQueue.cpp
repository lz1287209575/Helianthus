#include "MessageQueue.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"
#include "Shared/Common/StructuredLogger.h"

#include <algorithm>
#include <future>
#include <iostream>
#include <sstream>
#include <numeric>

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
    Shutdown();
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

    // 启动调度器线程
    SchedulerThread = std::thread(&MessageQueue::ProcessScheduledMessages, this);

    // 启动DLQ监控线程
    StopDeadLetterMonitor = false;
    DeadLetterMonitorThread = std::thread(&MessageQueue::ProcessDeadLetterMonitoring, this);

    // 启动指标监控线程
    StopMetricsMonitor = false;
    MetricsMonitorThread = std::thread(&MessageQueue::ProcessMetricsMonitoring, this);

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
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "指标监控线程停止");
}

}  // namespace Helianthus::MessageQueue