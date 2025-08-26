#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>
#include <unordered_map>

#include "Shared/MessageQueue/MessageTypes.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

using namespace Helianthus::MessageQueue;

// 简化的死信队列实现
class SimpleDeadLetterQueue {
private:
    std::queue<MessagePtr> deadLetterMessages;
    std::unordered_map<std::string, std::queue<MessagePtr>> queueMessages;
    std::unordered_map<std::string, QueueConfig> queueConfigs;
    std::atomic<MessageId> nextMessageId{1};
    
public:
    SimpleDeadLetterQueue() {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建简化死信队列");
    }
    
    MessageId GenerateMessageId() {
        return nextMessageId.fetch_add(1);
    }
    
    bool CreateQueue(const QueueConfig& config) {
        queueConfigs[config.Name] = config;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列: {}", config.Name);
        return true;
    }
    
    bool SendMessage(const std::string& queueName, MessagePtr message) {
        if (queueConfigs.find(queueName) == queueConfigs.end()) {
            return false;
        }
        
        message->Header.Id = GenerateMessageId();
        message->Status = MessageStatus::PENDING;
        queueMessages[queueName].push(message);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送消息到队列: {}, id: {}", queueName, message->Header.Id);
        return true;
    }
    
    bool ReceiveMessage(const std::string& queueName, MessagePtr& message) {
        if (queueMessages.find(queueName) == queueMessages.end() || queueMessages[queueName].empty()) {
            return false;
        }
        
        message = queueMessages[queueName].front();
        queueMessages[queueName].pop();
        message->Status = MessageStatus::DELIVERED;
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "从队列接收消息: {}, id: {}", queueName, message->Header.Id);
        return true;
    }
    
    bool RejectMessage(const std::string& queueName, MessageId messageId, bool requeue) {
        // 查找消息（简化实现）
        for (auto& [name, queue] : queueMessages) {
            if (name == queueName) {
                // 这里简化处理，实际应该从待确认列表中查找
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "拒绝消息: {}, id: {}", queueName, messageId);
                
                if (requeue) {
                    // 创建重试消息
                    auto retryMessage = std::make_shared<Message>(MessageType::TEXT, "重试消息");
                    retryMessage->Header.Id = messageId;
                    retryMessage->Header.RetryCount = 1;
                    retryMessage->Header.MaxRetries = 2;
                    retryMessage->Status = MessageStatus::PENDING;
                    
                    queue.push(retryMessage);
                    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息重新入队进行重试");
                } else {
                    // 移动到死信队列
                    auto deadMessage = std::make_shared<Message>(MessageType::TEXT, "死信消息");
                    deadMessage->Header.Id = messageId;
                    deadMessage->Header.DeadLetterReasonValue = DeadLetterReason::REJECTED;
                    deadMessage->Header.OriginalQueue = queueName;
                    deadMessage->Status = MessageStatus::DEAD_LETTER;
                    
                    deadLetterMessages.push(deadMessage);
                    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息移动到死信队列");
                }
                return true;
            }
        }
        return false;
    }
    
    std::vector<MessagePtr> GetDeadLetterMessages(const std::string& queueName, uint32_t maxCount) {
        std::vector<MessagePtr> messages;
        uint32_t count = 0;
        
        while (!deadLetterMessages.empty() && count < maxCount) {
            messages.push_back(deadLetterMessages.front());
            deadLetterMessages.pop();
            count++;
        }
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取死信消息: {} 条", count);
        return messages;
    }
};

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
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 死信队列核心逻辑测试 ===");
    
    // 创建简化的死信队列
    auto dlq = std::make_unique<SimpleDeadLetterQueue>();
    
    // 创建队列配置
    QueueConfig config;
    config.Name = "test_queue";
    config.Type = QueueType::STANDARD;
    config.MaxSize = 100;
    config.EnableDeadLetter = true;
    config.MaxRetries = 2;
    config.RetryDelayMs = 1000;
    
    // 创建队列
    if (!dlq->CreateQueue(config)) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "创建队列失败");
        return 1;
    }
    
    // 测试1：基本消息发送和接收
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试1：基本消息发送和接收 ===");
    
    auto testMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条测试消息");
    if (dlq->SendMessage(config.Name, testMessage)) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送消息成功");
    }
    
    MessagePtr receivedMessage;
    if (dlq->ReceiveMessage(config.Name, receivedMessage)) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收消息成功: {}", receivedMessage->Payload.AsString());
    }
    
    // 测试2：消息拒绝和重试
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试2：消息拒绝和重试 ===");
    
    auto retryMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条会重试的消息");
    retryMessage->Header.MaxRetries = 2;
    
    if (dlq->SendMessage(config.Name, retryMessage)) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送重试消息成功");
    }
    
    // 接收并拒绝消息
    if (dlq->ReceiveMessage(config.Name, receivedMessage)) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息，准备拒绝");
        dlq->RejectMessage(config.Name, receivedMessage->Header.Id, true);
    }
    
    // 再次接收（重试后的消息）
    if (dlq->ReceiveMessage(config.Name, receivedMessage)) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到重试消息，重试次数: {}", receivedMessage->Header.RetryCount);
        
        // 再次拒绝，这次不重试
        dlq->RejectMessage(config.Name, receivedMessage->Header.Id, false);
    }
    
    // 测试3：检查死信队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试3：检查死信队列 ===");
    
    auto deadMessages = dlq->GetDeadLetterMessages(config.Name, 10);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "死信队列中有 {} 条消息", deadMessages.size());
    
    for (const auto& msg : deadMessages) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "死信消息: id={}, reason={}, originalQueue={}", 
              msg->Header.Id, 
              static_cast<int>(msg->Header.DeadLetterReasonValue),
              msg->Header.OriginalQueue);
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 死信队列核心逻辑测试完成 ===");
    
    return 0;
}
