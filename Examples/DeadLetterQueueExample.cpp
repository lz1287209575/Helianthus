#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

using namespace Helianthus::MessageQueue;

int main()
{
    // 初始化日志系统
    Helianthus::Common::Logger::LoggerConfig logCfg;
    logCfg.Level = Helianthus::Common::LogLevel::INFO;
    logCfg.EnableConsole = true;
    logCfg.EnableFile = false;
    logCfg.UseAsync = false;
    Helianthus::Common::Logger::Initialize(logCfg);
    
    // 设置MQ分类的最小级别
    MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 死信队列示例 ===");
    
    // 创建消息队列实例
    auto Queue = std::make_unique<MessageQueue>();
    
    // 初始化消息队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化消息队列...");
    auto initResult = Queue->Initialize();
    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息队列初始化失败 code={}", static_cast<int>(initResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息队列初始化成功");
    
    // 创建测试队列配置（启用死信队列）
    QueueConfig config;
    config.Name = "test_dlq_queue";
    config.Type = QueueType::STANDARD;
    config.Persistence = PersistenceMode::MEMORY_ONLY;
    config.MaxSize = 100;
    config.MaxSizeBytes = 10 * 1024 * 1024;  // 10MB
    config.MessageTtlMs = 5000;  // 5秒TTL
    config.EnableDeadLetter = true;
    config.DeadLetterQueue = "test_dlq_queue_DLQ";
    config.MaxRetries = 2;  // 最大重试2次
    config.RetryDelayMs = 1000;  // 1秒重试延迟
    config.EnableRetryBackoff = true;  // 启用指数退避
    config.RetryBackoffMultiplier = 2.0;  // 退避倍数
    config.MaxRetryDelayMs = 10000;  // 最大重试延迟10秒
    config.DeadLetterTtlMs = 60000;  // 死信消息TTL 1分钟
    
    // 创建队列
    auto createResult = Queue->CreateQueue(config);
    if (createResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "创建队列失败 code={}", static_cast<int>(createResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列成功: {}", config.Name);
    
    // 测试1：过期消息自动移动到死信队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试1：过期消息 ===");
    
    auto expiredMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条会过期的消息");
    expiredMessage->Header.Priority = MessagePriority::NORMAL;
    expiredMessage->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    expiredMessage->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::system_clock::now().time_since_epoch())
                                           .count() + 2000;  // 2秒后过期
    
    auto sendResult = Queue->SendMessage(config.Name, expiredMessage);
    if (sendResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送过期消息成功 id={}", expiredMessage->Header.Id);
    }
    
    // 等待消息过期
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待消息过期...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 尝试接收消息（应该失败，因为消息已过期）
    MessagePtr receivedMessage;
    auto receiveResult = Queue->ReceiveMessage(config.Name, receivedMessage, 1000);
    if (receiveResult == QueueResult::TIMEOUT)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息已过期，无法接收");
    }
    
    // 检查死信队列
    std::vector<MessagePtr> deadLetterMessages;
    auto dlqResult = Queue->GetDeadLetterMessages(config.Name, deadLetterMessages, 10);
    if (dlqResult == QueueResult::SUCCESS && !deadLetterMessages.empty())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "死信队列中有 {} 条消息", deadLetterMessages.size());
        for (const auto& msg : deadLetterMessages)
        {
                     H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
               "死信消息: id={}, reason={}, originalQueue={}", 
               msg->Header.Id, 
               static_cast<int>(msg->Header.DeadLetterReasonValue),
               msg->Header.OriginalQueue);
        }
    }
    
    // 测试2：重试机制
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试2：重试机制 ===");
    
    auto retryMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条会重试的消息");
    retryMessage->Header.Priority = MessagePriority::NORMAL;
    retryMessage->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    retryMessage->Header.MaxRetries = 2;
    
    sendResult = Queue->SendMessage(config.Name, retryMessage);
    if (sendResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送重试消息成功 id={}", retryMessage->Header.Id);
    }
    
    // 接收消息并拒绝（触发重试）
    receiveResult = Queue->ReceiveMessage(config.Name, receivedMessage, 1000);
    if (receiveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息 id={}", receivedMessage->Header.Id);
        
        // 拒绝消息，启用重试
        auto rejectResult = Queue->RejectMessage(config.Name, receivedMessage->Header.Id, true);
        if (rejectResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息已拒绝，将进行重试");
        }
    }
    
    // 等待重试延迟
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待重试延迟...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 再次接收消息（重试后的消息）
    receiveResult = Queue->ReceiveMessage(config.Name, receivedMessage, 1000);
    if (receiveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "接收到重试消息 id={}, retryCount={}", 
              receivedMessage->Header.Id, 
              receivedMessage->Header.RetryCount);
        
        // 再次拒绝，超过最大重试次数
        auto rejectResult = Queue->RejectMessage(config.Name, receivedMessage->Header.Id, true);
        if (rejectResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息再次被拒绝，超过最大重试次数");
        }
    }
    
    // 等待最终重试延迟
    std::this_thread::sleep_for(std::chrono::seconds(4));
    
    // 检查死信队列（应该包含超过重试次数的消息）
    deadLetterMessages.clear();
    dlqResult = Queue->GetDeadLetterMessages(config.Name, deadLetterMessages, 10);
    if (dlqResult == QueueResult::SUCCESS && !deadLetterMessages.empty())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "死信队列中有 {} 条消息", deadLetterMessages.size());
        for (const auto& msg : deadLetterMessages)
        {
                         H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
                   "死信消息: id={}, reason={}, retryCount={}", 
                   msg->Header.Id, 
                   static_cast<int>(msg->Header.DeadLetterReasonValue),
                   msg->Header.RetryCount);
        }
    }
    
    // 测试3：死信消息重新入队
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试3：死信消息重新入队 ===");
    
    if (!deadLetterMessages.empty())
    {
        auto messageToRequeue = deadLetterMessages[0];
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "尝试重新入队消息 id={}", messageToRequeue->Header.Id);
        
        auto requeueResult = Queue->RequeueDeadLetterMessage(config.Name, messageToRequeue->Header.Id);
        if (requeueResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "死信消息重新入队成功");
            
            // 尝试接收重新入队的消息
            receiveResult = Queue->ReceiveMessage(config.Name, receivedMessage, 1000);
            if (receiveResult == QueueResult::SUCCESS)
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
                      "接收到重新入队的消息 id={}, retryCount={}", 
                      receivedMessage->Header.Id, 
                      receivedMessage->Header.RetryCount);
                
                // 确认消息
                auto ackResult = Queue->AcknowledgeMessage(config.Name, receivedMessage->Header.Id);
                if (ackResult == QueueResult::SUCCESS)
                {
                    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "重新入队的消息已确认");
                }
            }
        }
    }
    
    // 测试4：清空死信队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试4：清空死信队列 ===");
    
    auto purgeResult = Queue->PurgeDeadLetterQueue(config.Name);
    if (purgeResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "死信队列已清空");
    }
    
    // 获取队列统计信息
    QueueStats stats;
    auto statsResult = Queue->GetQueueStats(config.Name, stats);
    if (statsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "队列统计: totalMessages={}, processedMessages={}, failedMessages={}, deadLetterMessages={}, retriedMessages={}, expiredMessages={}, rejectedMessages={}", 
              stats.TotalMessages,
              stats.ProcessedMessages,
              stats.FailedMessages,
              stats.DeadLetterMessages,
              stats.RetriedMessages,
              stats.ExpiredMessages,
              stats.RejectedMessages);
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 死信队列示例完成 ===");
    
    return 0;
}
