#include <memory>
#include <string>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

using namespace Helianthus::MessageQueue;

int main()
{
    // 初始化项目 Logger：设置 INFO 级别，启用控制台，关闭异步，确保立即可见
    Helianthus::Common::Logger::LoggerConfig logCfg;
    logCfg.Level = Helianthus::Common::LogLevel::INFO;
    logCfg.EnableConsole = true;
    logCfg.EnableFile = false;
    logCfg.UseAsync = false;
    Helianthus::Common::Logger::Initialize(logCfg);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Logger initialized (console sync)");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "H_LOG smoke test");
    // 放宽 MQ 分类最小级别，确保 H_LOG 可见
    Helianthus::Common::LogCategory::SetCategoryMinVerbosity("MQ", Helianthus::Common::LogVerbosity::VeryVerbose);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::VeryVerbose, "H_LOG after set min verbosity");
    // 分类日志验证：应看到 [MQ]
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "分类日志验证：MQ 分类可见");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 消息队列持久化简单示例 ===");
    
    // 创建消息队列实例
    auto queue = std::make_unique<MessageQueue>();
    
    // 初始化消息队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Begin: queue->Initialize()");
    auto initResult = queue->Initialize();
    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息队列初始化失败 code={}", static_cast<int>(initResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息队列初始化成功");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Initialized OK");
    
    // 创建持久化队列配置
    QueueConfig config;
    config.Name = "test_persistent_queue";
    config.Type = QueueType::STANDARD;
    config.Persistence = PersistenceMode::DISK_PERSISTENT;
    config.MaxSize = 100;
    config.MaxSizeBytes = 10 * 1024 * 1024;  // 10MB
    
    // 创建队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Begin: CreateQueue({})", config.Name);
    auto createResult = queue->CreateQueue(config);
    if (createResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "创建队列失败 code={}", static_cast<int>(createResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建持久化队列成功 queue={}", config.Name);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "CreateQueue OK");
    
    // 发送测试消息
    for (int i = 1; i <= 3; ++i)
    {
        auto message = std::make_shared<Message>(MessageType::TEXT, 
            "测试消息 #" + std::to_string(i));
        
        message->Header.Priority = MessagePriority::NORMAL;
        message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "SendMessage begin id? content={}", message->Payload.AsString());
        auto sendResult = queue->SendMessage(config.Name, message);
        if (sendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送消息成功 id={} content={}",
                 static_cast<uint64_t>(message->Header.Id), message->Payload.AsString());
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "SendMessage OK id={}", static_cast<uint64_t>(message->Header.Id));
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送消息失败 code={}", static_cast<int>(sendResult));
        }
    }
    
    // 保存到磁盘
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Begin: SaveToDisk()");
    auto saveResult = queue->SaveToDisk();
    if (saveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息已保存到磁盘");
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "保存到磁盘失败 code={}", static_cast<int>(saveResult));
    }
    
    // 从磁盘加载
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Begin: LoadFromDisk()");
    auto loadResult = queue->LoadFromDisk();
    if (loadResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "从磁盘加载消息成功");
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "从磁盘加载失败 code={}", static_cast<int>(loadResult));
    }
    
    // 接收消息
    std::vector<MessagePtr> messages;
    auto receiveResult = queue->ReceiveBatchMessages(config.Name, messages, 10, 1000);
    
    if (receiveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到 {} 条消息", static_cast<uint32_t>(messages.size()));
        for (const auto& msg : messages)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "  - 消息ID={} 内容={}", static_cast<uint64_t>(msg->Header.Id), msg->Payload.AsString());
        }
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "接收消息失败 code={}", static_cast<int>(receiveResult));
    }
    
    // 关闭消息队列
    queue->Shutdown();
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 持久化示例完成 ===");
    Helianthus::Common::Logger::Shutdown();
    return 0;
}
