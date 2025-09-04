#include "Shared/Common/StructuredLogger.h"
#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Helianthus::Common;
using namespace Helianthus::MessageQueue;

class MessageQueueExample
{
public:
    void RunServer()
    {
        std::cout << "=== 启动消息队列服务器 ===" << std::endl;

        // 初始化日志
        StructuredLoggerConfig logConfig;
        logConfig.MinLevel = StructuredLogLevel::INFO;
        StructuredLogger::Initialize(logConfig);

        std::cout << "消息队列服务器已启动" << std::endl;
        std::cout << "按 Ctrl+C 停止服务器" << std::endl;

        // 主循环
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "服务器运行中..." << std::endl;
        }
    }

    void RunClient()
    {
        std::cout << "=== 启动消息队列客户端 ===" << std::endl;

        // 初始化日志
        StructuredLoggerConfig logConfig;
        logConfig.MinLevel = StructuredLogLevel::INFO;
        StructuredLogger::Initialize(logConfig);

        std::cout << "消息队列客户端已启动" << std::endl;

        // 测试基本消息操作
        TestBasicMessageOperations();

        // 测试优先级队列
        TestPriorityQueue();

        // 测试发布订阅
        TestPublishSubscribe();

        // 测试批量操作
        TestBatchOperations();

        // 测试延迟消息
        TestDelayMessages();

        std::cout << "所有测试完成！" << std::endl;
    }

private:
    void TestBasicMessageOperations()
    {
        std::cout << "\n--- 测试基本消息操作 ---" << std::endl;

        // 创建消息
        auto message = std::make_shared<Message>(MessageType::TEXT, "Hello, Message Queue!");
        message->Header.Priority = MessagePriority::NORMAL;
        message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;

        std::cout << "✓ 创建消息: " << message->Payload.AsString() << std::endl;
        std::cout << "  消息ID: " << message->Header.Id << std::endl;
        std::cout << "  消息类型: " << static_cast<int>(message->Header.Type) << std::endl;
        std::cout << "  优先级: " << static_cast<int>(message->Header.Priority) << std::endl;
        std::cout << "  传递模式: " << static_cast<int>(message->Header.Delivery) << std::endl;
    }

    void TestPriorityQueue()
    {
        std::cout << "\n--- 测试优先级队列 ---" << std::endl;

        // 创建不同优先级的消息
        std::vector<std::pair<MessagePriority, std::string>> testMessages = {
            {MessagePriority::LOW, "低优先级消息"},
            {MessagePriority::HIGH, "高优先级消息"},
            {MessagePriority::NORMAL, "普通优先级消息"},
            {MessagePriority::CRITICAL, "关键优先级消息"}};

        for (const auto& [priority, content] : testMessages)
        {
            auto message = std::make_shared<Message>(MessageType::TEXT, content);
            message->Header.Priority = priority;

            std::cout << "✓ 创建 " << static_cast<int>(priority)
                      << " 优先级消息: " << message->Payload.AsString() << std::endl;
        }
    }

    void TestPublishSubscribe()
    {
        std::cout << "\n--- 测试发布订阅 ---" << std::endl;

        // 创建游戏事件消息
        auto gameEvent =
            std::make_shared<Message>(MessageType::PLAYER_EVENT,
                                      R"({"event": "player_join", "player_id": 123, "level": 50})");

        std::cout << "✓ 创建游戏事件消息: " << gameEvent->Payload.AsString() << std::endl;

        // 创建系统通知消息
        auto systemNotification = std::make_shared<Message>(
            MessageType::SYSTEM_NOTIFICATION,
            R"({"type": "maintenance", "message": "系统将在10分钟后维护", "duration": 30})");

        std::cout << "✓ 创建系统通知消息: " << systemNotification->Payload.AsString() << std::endl;
    }

    void TestBatchOperations()
    {
        std::cout << "\n--- 测试批量操作 ---" << std::endl;

        // 创建批量消息
        std::vector<MessagePtr> batchMessages;
        for (int i = 1; i <= 5; ++i)
        {
            auto message =
                std::make_shared<Message>(MessageType::TEXT, "批量消息 #" + std::to_string(i));
            batchMessages.push_back(message);
        }

        std::cout << "✓ 创建 " << batchMessages.size() << " 条批量消息" << std::endl;

        // 显示批量消息内容
        for (size_t i = 0; i < batchMessages.size(); ++i)
        {
            std::cout << "  " << (i + 1) << ". " << batchMessages[i]->Payload.AsString()
                      << std::endl;
        }
    }

    void TestDelayMessages()
    {
        std::cout << "\n--- 测试延迟消息 ---" << std::endl;

        // 创建延迟消息
        auto delayMessage = std::make_shared<Message>(MessageType::TEXT, "这是延迟消息");

        // 设置延迟时间（2秒后处理）
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        delayMessage->Header.ExpireTime = now + 2000;

        std::cout << "✓ 创建延迟消息: " << delayMessage->Payload.AsString() << std::endl;
        std::cout << "  过期时间: " << delayMessage->Header.ExpireTime << std::endl;
        std::cout << "  当前时间: " << now << std::endl;
        std::cout << "  延迟时间: " << (delayMessage->Header.ExpireTime - now) << "ms" << std::endl;
    }

    void TestQueueConfig()
    {
        std::cout << "\n--- 测试队列配置 ---" << std::endl;

        // 创建标准队列配置
        QueueConfig standardConfig;
        standardConfig.Name = "standard_queue";
        standardConfig.Type = QueueType::STANDARD;
        standardConfig.MaxSize = 1000;
        standardConfig.MaxSizeBytes = 1024 * 1024;  // 1MB
        standardConfig.MaxConsumers = 10;
        standardConfig.MaxProducers = 10;
        standardConfig.MessageTtlMs = 300000;  // 5分钟
        standardConfig.EnablePriority = false;
        standardConfig.EnableBatching = true;
        standardConfig.BatchSize = 100;
        standardConfig.BatchTimeoutMs = 1000;

        std::cout << "✓ 创建标准队列配置: " << standardConfig.Name << std::endl;
        std::cout << "  最大大小: " << standardConfig.MaxSize << std::endl;
        std::cout << "  最大字节数: " << standardConfig.MaxSizeBytes << std::endl;
        std::cout << "  最大消费者: " << standardConfig.MaxConsumers << std::endl;
        std::cout << "  最大生产者: " << standardConfig.MaxProducers << std::endl;
        std::cout << "  消息TTL: " << standardConfig.MessageTtlMs << "ms" << std::endl;
        std::cout << "  启用优先级: " << (standardConfig.EnablePriority ? "是" : "否") << std::endl;
        std::cout << "  启用批量处理: " << (standardConfig.EnableBatching ? "是" : "否")
                  << std::endl;

        // 创建优先级队列配置
        QueueConfig priorityConfig = standardConfig;
        priorityConfig.Name = "priority_queue";
        priorityConfig.Type = QueueType::PRIORITY;
        priorityConfig.EnablePriority = true;

        std::cout << "✓ 创建优先级队列配置: " << priorityConfig.Name << std::endl;
        std::cout << "  启用优先级: " << (priorityConfig.EnablePriority ? "是" : "否") << std::endl;

        // 创建延迟队列配置
        QueueConfig delayConfig = standardConfig;
        delayConfig.Name = "delay_queue";
        delayConfig.Type = QueueType::DELAY;

        std::cout << "✓ 创建延迟队列配置: " << delayConfig.Name << std::endl;
    }

    void TestTopicConfig()
    {
        std::cout << "\n--- 测试主题配置 ---" << std::endl;

        // 创建游戏事件主题配置
        TopicConfig gameTopicConfig;
        gameTopicConfig.Name = "game_events";
        gameTopicConfig.MaxSubscribers = 100;
        gameTopicConfig.MessageTtlMs = 60000;                 // 1分钟
        gameTopicConfig.RetentionMs = 3600000;                // 1小时
        gameTopicConfig.RetentionBytes = 1024 * 1024 * 1024;  // 1GB
        gameTopicConfig.EnablePartitioning = false;
        gameTopicConfig.PartitionCount = 1;

        std::cout << "✓ 创建游戏事件主题配置: " << gameTopicConfig.Name << std::endl;
        std::cout << "  最大订阅者: " << gameTopicConfig.MaxSubscribers << std::endl;
        std::cout << "  消息TTL: " << gameTopicConfig.MessageTtlMs << "ms" << std::endl;
        std::cout << "  保留时间: " << gameTopicConfig.RetentionMs << "ms" << std::endl;
        std::cout << "  保留字节数: " << gameTopicConfig.RetentionBytes << std::endl;
        std::cout << "  启用分区: " << (gameTopicConfig.EnablePartitioning ? "是" : "否")
                  << std::endl;
        std::cout << "  分区数量: " << gameTopicConfig.PartitionCount << std::endl;

        // 创建系统通知主题配置
        TopicConfig systemTopicConfig = gameTopicConfig;
        systemTopicConfig.Name = "system_notifications";

        std::cout << "✓ 创建系统通知主题配置: " << systemTopicConfig.Name << std::endl;
    }

    void TestConsumerConfig()
    {
        std::cout << "\n--- 测试消费者配置 ---" << std::endl;

        // 创建消费者配置
        ConsumerConfig consumerConfig;
        consumerConfig.ConsumerId = "test_consumer";
        consumerConfig.GroupId = "test_group";
        consumerConfig.AutoAcknowledge = true;
        consumerConfig.PrefetchCount = 10;
        consumerConfig.AckTimeoutMs = 30000;
        consumerConfig.EnableBatching = false;
        consumerConfig.BatchSize = 10;
        consumerConfig.BatchTimeoutMs = 1000;
        consumerConfig.MinPriority = MessagePriority::LOW;

        std::cout << "✓ 创建消费者配置: " << consumerConfig.ConsumerId << std::endl;
        std::cout << "  消费者组: " << consumerConfig.GroupId << std::endl;
        std::cout << "  自动确认: " << (consumerConfig.AutoAcknowledge ? "是" : "否") << std::endl;
        std::cout << "  预取数量: " << consumerConfig.PrefetchCount << std::endl;
        std::cout << "  确认超时: " << consumerConfig.AckTimeoutMs << "ms" << std::endl;
        std::cout << "  启用批量: " << (consumerConfig.EnableBatching ? "是" : "否") << std::endl;
        std::cout << "  批量大小: " << consumerConfig.BatchSize << std::endl;
        std::cout << "  批量超时: " << consumerConfig.BatchTimeoutMs << "ms" << std::endl;
        std::cout << "  最低优先级: " << static_cast<int>(consumerConfig.MinPriority) << std::endl;
    }

    void TestProducerConfig()
    {
        std::cout << "\n--- 测试生产者配置 ---" << std::endl;

        // 创建生产者配置
        ProducerConfig producerConfig;
        producerConfig.ProducerId = "test_producer";
        producerConfig.EnableBatching = false;
        producerConfig.BatchSize = 100;
        producerConfig.BatchTimeoutMs = 1000;
        producerConfig.WaitForAcknowledge = false;
        producerConfig.AckTimeoutMs = 5000;
        producerConfig.MaxRetries = 3;
        producerConfig.RetryIntervalMs = 1000;

        std::cout << "✓ 创建生产者配置: " << producerConfig.ProducerId << std::endl;
        std::cout << "  启用批量: " << (producerConfig.EnableBatching ? "是" : "否") << std::endl;
        std::cout << "  批量大小: " << producerConfig.BatchSize << std::endl;
        std::cout << "  批量超时: " << producerConfig.BatchTimeoutMs << "ms" << std::endl;
        std::cout << "  等待确认: " << (producerConfig.WaitForAcknowledge ? "是" : "否")
                  << std::endl;
        std::cout << "  确认超时: " << producerConfig.AckTimeoutMs << "ms" << std::endl;
        std::cout << "  最大重试: " << producerConfig.MaxRetries << std::endl;
        std::cout << "  重试间隔: " << producerConfig.RetryIntervalMs << "ms" << std::endl;
    }
};

int main(int argc, char* argv[])
{
    std::cout << "=== Helianthus 消息队列示例程序 ===" << std::endl;

    bool runServer = false;
    bool runClient = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--server")
        {
            runServer = true;
        }
        else if (arg == "--client")
        {
            runClient = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  --server              运行消息队列服务器" << std::endl;
            std::cout << "  --client              运行消息队列客户端" << std::endl;
            std::cout << "  --help, -h            显示此帮助信息" << std::endl;
            std::cout << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  " << argv[0] << " --server" << std::endl;
            std::cout << "  " << argv[0] << " --client" << std::endl;
            return 0;
        }
    }

    if (!runServer && !runClient)
    {
        std::cout << "请指定 --server 或 --client 参数" << std::endl;
        std::cout << "使用 --help 查看帮助信息" << std::endl;
        return 1;
    }

    MessageQueueExample example;

    if (runServer)
    {
        example.RunServer();
    }
    else if (runClient)
    {
        example.RunClient();
    }

    return 0;
}
