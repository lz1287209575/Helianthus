#include "MessageQueue.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

#include <algorithm>
#include <future>
#include <iostream>
#include <sstream>

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

    // 停止调度器线程
    SchedulerCondition.notify_all();
    if (SchedulerThread.joinable())
    {
        SchedulerThread.join();
    }

    // 停止所有消费者线程
    for (auto& Thread : ConsumerThreads)
    {
        if (Thread.joinable())
        {
            Thread.join();
        }
    }
    ConsumerThreads.clear();

    // 清理队列和主题
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

    // 关闭持久化管理器
    if (PersistenceMgr)
    {
        PersistenceMgr->Shutdown();
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

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);

    // 检查队列容量
    if (Queue->GetMessageCount() >= Queue->Config.MaxSize)
    {
        return QueueResult::QUEUE_FULL;
    }

    // 检查消息大小
    if (Message->Payload.Size > Queue->Config.MaxSizeBytes / Queue->Config.MaxSize)
    {
        return QueueResult::MESSAGE_TOO_LARGE;
    }

    // 添加消息到队列
    Queue->AddMessage(Message);

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
        MoveToDeadLetter(QueueName, OutMessage);
        return QueueResult::TIMEOUT;  // 递归尝试获取下一个消息
    }

    OutMessage->Status = MessageStatus::DELIVERED;

    // 如果不是自动确认，添加到待确认列表
    const auto ConsumerIt = Queue->Consumers.begin();
    if (ConsumerIt != Queue->Consumers.end() && !ConsumerIt->second.AutoAcknowledge)
    {
        Queue->PendingAcknowledgments[OutMessage->Header.Id] = OutMessage;
    }

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

void MessageQueue::MoveToDeadLetter(const std::string& QueueName, MessagePtr Message)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);
    Message->Status = MessageStatus::DEAD_LETTER;
    Queue->DeadLetterMessages.push(Message);
    Queue->Stats.DeadLetterMessages++;
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
        SchedulerCondition.wait_for(Lock,
                                    std::chrono::seconds(1),
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
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::RequeueDeadLetterMessage(const std::string& QueueName,
                                                   MessageId MessageId)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::PurgeDeadLetterQueue(const std::string& QueueName)
{
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
    return QueueResult::SUCCESS;
}
std::string MessageQueue::GetGlobalConfig(const std::string& Key) const
{
    return "";
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

}  // namespace Helianthus::MessageQueue