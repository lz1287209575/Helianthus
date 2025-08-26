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
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 简单发送测试 ===");
    
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
    config.Name = "test_simple_queue";
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
    
    // 测试1：发送普通消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试1：发送普通消息 ===");
    
    auto normalMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条普通消息");
    normalMessage->Header.Priority = MessagePriority::NORMAL;
    normalMessage->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    
    auto sendResult = queue->SendMessage(config.Name, normalMessage);
    if (sendResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送普通消息成功 id={}", normalMessage->Header.Id);
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送普通消息失败: {}", static_cast<int>(sendResult));
    }
    
    // 测试2：发送重试消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试2：发送重试消息 ===");
    
    auto retryMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条会重试的消息");
    retryMessage->Header.Priority = MessagePriority::NORMAL;
    retryMessage->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    retryMessage->Header.MaxRetries = 2;
    
    sendResult = queue->SendMessage(config.Name, retryMessage);
    if (sendResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送重试消息成功 id={}", retryMessage->Header.Id);
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送重试消息失败: {}", static_cast<int>(sendResult));
    }
    
    // 测试3：发送过期消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试3：发送过期消息 ===");
    
    auto expiredMessage = std::make_shared<Message>(MessageType::TEXT, "这是一条过期消息");
    expiredMessage->Header.Priority = MessagePriority::NORMAL;
    expiredMessage->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    expiredMessage->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 1000;  // 1秒后过期
    
    sendResult = queue->SendMessage(config.Name, expiredMessage);
    if (sendResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送过期消息成功 id={}", expiredMessage->Header.Id);
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送过期消息失败: {}", static_cast<int>(sendResult));
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 简单发送测试完成 ===");
    
    return 0;
}
