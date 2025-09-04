#include "Shared/Common/StructuredLogger.h"
#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Helianthus::Common;
using namespace Helianthus::MessageQueue;

class MessageQueuePersistenceExample
{
public:
    void RunPersistenceDemo()
    {
        std::cout << "=== Helianthus 消息队列持久化演示 ===" << std::endl;

        // 初始化日志
        StructuredLoggerConfig logConfig;
        logConfig.MinLevel = StructuredLogLevel::INFO;
        StructuredLogger::Initialize(logConfig);

        // 创建消息队列实例
        auto MessageQueue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();

        // 初始化消息队列
        auto InitResult = MessageQueue->Initialize();
        if (InitResult != QueueResult::SUCCESS)
        {
            std::cout << "❌ 消息队列初始化失败: " << static_cast<int>(InitResult) << std::endl;
            return;
        }

        std::cout << "✅ 消息队列初始化成功" << std::endl;

        // 创建持久化队列配置
        QueueConfig PersistenceConfig;
        PersistenceConfig.Name = "persistent_queue";
        PersistenceConfig.Type = QueueType::STANDARD;
        PersistenceConfig.Persistence = PersistenceMode::DISK_PERSISTENT;
        PersistenceConfig.MaxSize = 1000;
        PersistenceConfig.MaxSizeBytes = 100 * 1024 * 1024;  // 100MB
        PersistenceConfig.MaxConsumers = 10;
        PersistenceConfig.MaxProducers = 10;
        PersistenceConfig.MessageTtlMs = 300000;  // 5分钟
        PersistenceConfig.EnableDeadLetter = true;
        PersistenceConfig.DeadLetterQueue = "dead_letter_queue";
        PersistenceConfig.EnablePriority = false;
        PersistenceConfig.EnableBatching = true;
        PersistenceConfig.BatchSize = 100;
        PersistenceConfig.BatchTimeoutMs = 1000;

        // 创建队列
        auto CreateResult = MessageQueue->CreateQueue(PersistenceConfig);
        if (CreateResult != QueueResult::SUCCESS)
        {
            std::cout << "❌ 创建队列失败: " << static_cast<int>(CreateResult) << std::endl;
            return;
        }

        std::cout << "✅ 创建持久化队列成功: " << PersistenceConfig.Name << std::endl;

        // 演示1: 发送消息并持久化
        DemoMessagePersistence(MessageQueue.get(), PersistenceConfig.Name);

        // 演示2: 重启后恢复消息
        DemoMessageRecovery(MessageQueue.get(), PersistenceConfig.Name);

        // 演示3: 批量消息持久化
        DemoBatchPersistence(MessageQueue.get(), PersistenceConfig.Name);

        // 演示4: 死信队列处理
        DemoDeadLetterQueue(MessageQueue.get(), PersistenceConfig.Name);

        // 演示5: 持久化统计信息
        DemoPersistenceStats(MessageQueue.get(), PersistenceConfig.Name);

        // 关闭消息队列
        MessageQueue->Shutdown();

        std::cout << "=== 持久化演示完成 ===" << std::endl;
    }

private:
    void DemoMessagePersistence(Helianthus::MessageQueue::MessageQueue* Queue,
                                const std::string& QueueName)
    {
        std::cout << "\n--- 演示1: 消息持久化 ---" << std::endl;

        // 发送多条消息
        for (int i = 1; i <= 5; ++i)
        {
            auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
                MessageType::TEXT,
                "持久化消息 #" + std::to_string(i) + " - " + std::to_string(GetCurrentTimestamp()));

            Message->Header.Priority = MessagePriority::NORMAL;
            Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
            Message->Header.ExpireTime = GetCurrentTimestamp() + 300000;  // 5分钟后过期

            auto SendResult = Queue->SendMessage(QueueName, Message);
            if (SendResult == QueueResult::SUCCESS)
            {
                std::cout << "✅ 发送消息成功: " << Message->Header.Id << " - "
                          << Message->Payload.AsString() << std::endl;
            }
            else
            {
                std::cout << "❌ 发送消息失败: " << static_cast<int>(SendResult) << std::endl;
            }
        }

        // 保存到磁盘
        auto SaveResult = Queue->SaveToDisk();
        if (SaveResult == QueueResult::SUCCESS)
        {
            std::cout << "✅ 消息已保存到磁盘" << std::endl;
        }
        else
        {
            std::cout << "❌ 保存到磁盘失败: " << static_cast<int>(SaveResult) << std::endl;
        }
    }

    void DemoMessageRecovery(Helianthus::MessageQueue::MessageQueue* Queue,
                             const std::string& QueueName)
    {
        std::cout << "\n--- 演示2: 消息恢复 ---" << std::endl;

        // 模拟重启：清空内存中的消息
        std::cout << "🔄 模拟系统重启..." << std::endl;

        // 从磁盘加载消息
        auto LoadResult = Queue->LoadFromDisk();
        if (LoadResult == QueueResult::SUCCESS)
        {
            std::cout << "✅ 从磁盘恢复消息成功" << std::endl;

            // 接收所有恢复的消息
            std::vector<MessagePtr> RecoveredMessages;
            auto ReceiveResult =
                Queue->ReceiveBatchMessages(QueueName, RecoveredMessages, 10, 1000);

            if (ReceiveResult == QueueResult::SUCCESS)
            {
                std::cout << "✅ 接收到 " << RecoveredMessages.size()
                          << " 条恢复的消息:" << std::endl;
                for (const auto& Message : RecoveredMessages)
                {
                    std::cout << "  - 消息ID: " << Message->Header.Id
                              << ", 内容: " << Message->Payload.AsString() << std::endl;
                }
            }
        }
        else
        {
            std::cout << "❌ 从磁盘恢复消息失败: " << static_cast<int>(LoadResult) << std::endl;
        }
    }

    void DemoBatchPersistence(Helianthus::MessageQueue::MessageQueue* Queue,
                              const std::string& QueueName)
    {
        std::cout << "\n--- 演示3: 批量消息持久化 ---" << std::endl;

        // 创建批量消息
        std::vector<MessagePtr> BatchMessages;
        for (int i = 1; i <= 10; ++i)
        {
            auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
                MessageType::TEXT,
                "批量消息 #" + std::to_string(i) + " - " + std::to_string(GetCurrentTimestamp()));

            Message->Header.Priority = MessagePriority::HIGH;
            Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;

            BatchMessages.push_back(Message);
        }

        // 批量发送消息
        auto SendResult = Queue->SendBatchMessages(QueueName, BatchMessages);
        if (SendResult == QueueResult::SUCCESS)
        {
            std::cout << "✅ 批量发送 " << BatchMessages.size() << " 条消息成功" << std::endl;
        }
        else
        {
            std::cout << "❌ 批量发送失败: " << static_cast<int>(SendResult) << std::endl;
        }

        // 保存到磁盘
        auto SaveResult = Queue->SaveToDisk();
        if (SaveResult == QueueResult::SUCCESS)
        {
            std::cout << "✅ 批量消息已保存到磁盘" << std::endl;
        }
    }

    void DemoDeadLetterQueue(MessageQueue* Queue, const std::string& QueueName)
    {
        std::cout << "\n--- 演示4: 死信队列处理 ---" << std::endl;

        // 创建死信队列
        QueueConfig DeadLetterConfig;
        DeadLetterConfig.Name = "dead_letter_queue";
        DeadLetterConfig.Type = QueueType::DEAD_LETTER;
        DeadLetterConfig.Persistence = PersistenceMode::DISK_PERSISTENT;
        DeadLetterConfig.MaxSize = 100;
        DeadLetterConfig.MaxSizeBytes = 10 * 1024 * 1024;  // 10MB

        auto CreateResult = Queue->CreateQueue(DeadLetterConfig);
        if (CreateResult == QueueResult::SUCCESS)
        {
            std::cout << "✅ 创建死信队列成功" << std::endl;
        }

        // 发送一些过期消息（模拟无法处理的消息）
        for (int i = 1; i <= 3; ++i)
        {
            auto Msg = std::make_shared<Helianthus::MessageQueue::Message>(
                MessageType::TEXT, "过期消息 #" + std::to_string(i));

            Msg->Header.Priority = MessagePriority::LOW;
            Msg->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
            Msg->Header.ExpireTime = GetCurrentTimestamp() - 1000;  // 已过期

            auto SendResult = Queue->SendMessage(QueueName, Msg);
            if (SendResult == QueueResult::SUCCESS)
            {
                std::cout << "✅ 发送过期消息: " << Msg->Header.Id << std::endl;
            }
        }

        // 处理消息（过期消息会被移动到死信队列）
        std::vector<MessagePtr> ProcessedMessages;
        auto ReceiveResult = Queue->ReceiveBatchMessages(QueueName, ProcessedMessages, 10, 1000);

        if (ReceiveResult == QueueResult::SUCCESS)
        {
            std::cout << "✅ 处理了 " << ProcessedMessages.size() << " 条消息" << std::endl;

            // 检查死信队列
            std::vector<MessagePtr> DeadLetterMessages;
            auto DeadLetterResult =
                Queue->ReceiveBatchMessages("dead_letter_queue", DeadLetterMessages, 10, 1000);

            if (DeadLetterResult == QueueResult::SUCCESS && !DeadLetterMessages.empty())
            {
                std::cout << "✅ 死信队列中有 " << DeadLetterMessages.size()
                          << " 条消息:" << std::endl;
                for (const auto& Message : DeadLetterMessages)
                {
                    std::cout << "  - 死信消息ID: " << Message->Header.Id
                              << ", 内容: " << Message->Payload.AsString() << std::endl;
                }
            }
        }
    }

    void DemoPersistenceStats(MessageQueue* Queue, const std::string& QueueName)
    {
        std::cout << "\n--- 演示5: 持久化统计信息 ---" << std::endl;

        // 获取队列统计信息
        QueueStats Stats;
        auto StatsResult = Queue->GetQueueStats(QueueName, Stats);

        if (StatsResult == QueueResult::SUCCESS)
        {
            std::cout << "📊 队列统计信息:" << std::endl;
            std::cout << "  - 总消息数: " << Stats.TotalMessages << std::endl;
            std::cout << "  - 待处理消息数: " << Stats.PendingMessages << std::endl;
            std::cout << "  - 已处理消息数: " << Stats.ProcessedMessages << std::endl;
            std::cout << "  - 失败消息数: " << Stats.FailedMessages << std::endl;
            std::cout << "  - 死信消息数: " << Stats.DeadLetterMessages << std::endl;
            std::cout << "  - 总字节数: " << Stats.TotalBytes << std::endl;
            std::cout << "  - 平均延迟: " << Stats.AverageLatencyMs << "ms" << std::endl;
            std::cout << "  - 吞吐量: " << Stats.ThroughputPerSecond << " msg/s" << std::endl;
        }

        // 获取全局统计信息
        QueueStats GlobalStats;
        auto GlobalStatsResult = Queue->GetGlobalStats(GlobalStats);

        if (GlobalStatsResult == QueueResult::SUCCESS)
        {
            std::cout << "📊 全局统计信息:" << std::endl;
            std::cout << "  - 总消息数: " << GlobalStats.TotalMessages << std::endl;
            std::cout << "  - 总字节数: " << GlobalStats.TotalBytes << std::endl;
            std::cout << "  - 平均延迟: " << GlobalStats.AverageLatencyMs << "ms" << std::endl;
        }

        // 获取队列诊断信息
        auto Diagnostics = Queue->GetQueueDiagnostics(QueueName);
        if (!Diagnostics.empty())
        {
            std::cout << "🔍 队列诊断信息:" << std::endl;
            for (const auto& Diagnostic : Diagnostics)
            {
                std::cout << "  - " << Diagnostic << std::endl;
            }
        }
    }

    MessageTimestamp GetCurrentTimestamp()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
};

int main(int argc, char* argv[])
{
    std::cout << "=== Helianthus 消息队列持久化示例程序 ===" << std::endl;

    bool runDemo = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--demo")
        {
            runDemo = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  --demo              运行持久化演示" << std::endl;
            std::cout << "  --help, -h          显示此帮助信息" << std::endl;
            std::cout << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  " << argv[0] << " --demo" << std::endl;
            return 0;
        }
    }

    if (!runDemo)
    {
        std::cout << "请指定 --demo 参数运行演示" << std::endl;
        std::cout << "使用 --help 查看帮助信息" << std::endl;
        return 1;
    }

    MessageQueuePersistenceExample example;
    example.RunPersistenceDemo();

    return 0;
}
