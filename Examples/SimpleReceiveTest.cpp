#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include "Shared/MessageQueue/MessageQueue.h"
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
    MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 简单接收测试 ===");
    
    // 创建消息队列实例
    auto queue = std::make_unique<MessageQueue>();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建消息队列实例");
    
    // 初始化消息队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化消息队列...");
    auto initResult = queue->Initialize();
    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息队列初始化失败: {}", static_cast<int>(initResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息队列初始化成功");
    
    // 创建队列配置
    QueueConfig config;
    config.Name = "test_receive_queue";
    config.Type = QueueType::STANDARD;
    config.Persistence = PersistenceMode::MEMORY_ONLY;  // 使用内存模式避免文件持久化问题
    config.MaxSize = 1000;
    config.MaxSizeBytes = 1024 * 1024;  // 1MB
    config.MessageTtlMs = 30000;  // 30秒
    config.EnableDeadLetter = true;
    config.EnablePriority = false;
    config.EnableBatching = false;
    config.MaxRetries = 3;
    config.RetryDelayMs = 1000;
    config.EnableRetryBackoff = true;
    config.RetryBackoffMultiplier = 2.0;
    config.MaxRetryDelayMs = 10000;
    config.DeadLetterTtlMs = 86400000;  // 24小时
    
    // 创建队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列: {}", config.Name);
    auto createResult = queue->CreateQueue(config);
    if (createResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "创建队列失败: {}", static_cast<int>(createResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列成功: {}", config.Name);
    
    // 发送一些测试消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送测试消息...");
    
    for (int i = 1; i <= 3; ++i)
    {
        auto message = std::make_shared<Message>(MessageType::TEXT, "测试消息 " + std::to_string(i));
        message->Header.Priority = MessagePriority::NORMAL;
        message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        
        auto sendResult = queue->SendMessage(config.Name, message);
        if (sendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送消息成功 id={}", message->Header.Id);
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送消息失败: {}", static_cast<int>(sendResult));
        }
    }
    
    // 测试1：接收普通消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试1：接收普通消息 ===");
    
    MessagePtr receivedMessage;
    auto receiveResult = queue->ReceiveMessage(config.Name, receivedMessage, 1000);
    if (receiveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息 id={}", receivedMessage->Header.Id);
        
        // 确认消息
        auto ackResult = queue->AcknowledgeMessage(config.Name, receivedMessage->Header.Id);
        if (ackResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息确认成功");
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息确认失败: {}", static_cast<int>(ackResult));
        }
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "接收消息失败: {}", static_cast<int>(receiveResult));
    }
    
    // 测试2：接收第二条消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试2：接收第二条消息 ===");
    
    receiveResult = queue->ReceiveMessage(config.Name, receivedMessage, 1000);
    if (receiveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息 id={}", receivedMessage->Header.Id);
        
        // 确认消息
        auto ackResult = queue->AcknowledgeMessage(config.Name, receivedMessage->Header.Id);
        if (ackResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息确认成功");
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息确认失败: {}", static_cast<int>(ackResult));
        }
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "接收消息失败: {}", static_cast<int>(receiveResult));
    }
    
    // 测试3：接收第三条消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试3：接收第三条消息 ===");
    
    receiveResult = queue->ReceiveMessage(config.Name, receivedMessage, 1000);
    if (receiveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息 id={}", receivedMessage->Header.Id);
        
        // 确认消息
        auto ackResult = queue->AcknowledgeMessage(config.Name, receivedMessage->Header.Id);
        if (ackResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息确认成功");
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息确认失败: {}", static_cast<int>(ackResult));
        }
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "接收消息失败: {}", static_cast<int>(receiveResult));
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 简单接收测试完成 ===");
    
    return 0;
}
