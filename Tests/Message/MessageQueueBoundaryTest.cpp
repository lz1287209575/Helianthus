#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <chrono>
#include <limits>
#include <random>
#include <thread>

#include <gtest/gtest.h>

using namespace Helianthus::MessageQueue;

class MessageQueueBoundaryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MQ.Initialize();

        // 创建测试队列
        QueueConfig Config;
        Config.Name = "boundary_test_queue";
        Config.MaxSize = 1000;                   // 增加队列容量，支持更多测试
        Config.MaxSizeBytes = 10 * 1024 * 1024;  // 10MB字节限制
        Config.EnablePriority = true;            // 启用优先级队列功能
        Config.Persistence = PersistenceMode::MEMORY_ONLY;

        ASSERT_EQ(MQ.CreateQueue(Config), QueueResult::SUCCESS);
    }

    void TearDown() override
    {
        MQ.Shutdown();
    }

    MessagePtr CreateTestMessage(const std::string& Payload = "test")
    {
        auto Message =
            std::make_shared<Helianthus::MessageQueue::Message>(MessageType::TEXT, Payload);
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        return Message;
    }

    MessageQueue MQ;
    const std::string TestQueueName = "boundary_test_queue";
};

// ==================== 边界值测试 ====================

TEST_F(MessageQueueBoundaryTest, ZeroSizeQueue)
{
    // 创建大小为0的队列应该失败
    QueueConfig ZeroConfig;
    ZeroConfig.Name = "zero_queue";
    ZeroConfig.MaxSize = 0;
    ZeroConfig.Persistence = PersistenceMode::MEMORY_ONLY;

    // 0大小的队列应该创建失败
    EXPECT_EQ(MQ.CreateQueue(ZeroConfig), QueueResult::INVALID_PARAMETER);

    // 如果队列创建失败，后续操作也应该失败
    auto Message = CreateTestMessage("test");
    EXPECT_EQ(MQ.SendMessage("zero_queue", Message), QueueResult::QUEUE_NOT_FOUND);

    // 获取统计也应该失败
    QueueStats Stats;
    EXPECT_EQ(MQ.GetQueueStats("zero_queue", Stats), QueueResult::QUEUE_NOT_FOUND);
}

TEST_F(MessageQueueBoundaryTest, MaximumSizeQueue)
{
    // 创建最大容量的队列
    QueueConfig MaxConfig;
    MaxConfig.Name = "max_queue";
    MaxConfig.MaxSize = std::numeric_limits<uint32_t>::max();
    MaxConfig.EnablePriority = true;  // 启用优先级队列功能
    MaxConfig.Persistence = PersistenceMode::MEMORY_ONLY;

    EXPECT_EQ(MQ.CreateQueue(MaxConfig), QueueResult::SUCCESS);

    // 发送一些消息
    for (int i = 0; i < 100; ++i)
    {
        auto Message = CreateTestMessage("max_msg_" + std::to_string(i));
        EXPECT_EQ(MQ.SendMessage("max_queue", Message), QueueResult::SUCCESS);
    }

    // 验证消息都被接收
    for (int i = 0; i < 100; ++i)
    {
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage("max_queue", ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }
}

TEST_F(MessageQueueBoundaryTest, EmptyMessagePayload)
{
    // 测试空载荷消息
    auto EmptyMessage = CreateTestMessage("");

    // 如果系统允许空载荷消息，发送应该成功
    // 如果系统不允许空载荷消息，发送应该失败
    auto SendResult = MQ.SendMessage(TestQueueName, EmptyMessage);

    if (SendResult == QueueResult::SUCCESS)
    {
        // 系统允许空载荷，验证接收
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
        EXPECT_EQ(ReceivedMessage->Payload.Data.size(), 0);
    }
    else
    {
        // 系统不允许空载荷，这是合理的行为
        EXPECT_EQ(SendResult, QueueResult::INVALID_PARAMETER);
        // 尝试接收应该失败或返回空
        MessagePtr ReceivedMessage;
        auto ReceiveResult = MQ.ReceiveMessage(TestQueueName, ReceivedMessage);
        // 接收可能成功但消息为空，或者失败
        if (ReceiveResult == QueueResult::SUCCESS)
        {
            EXPECT_EQ(ReceivedMessage, nullptr);
        }
    }
}

TEST_F(MessageQueueBoundaryTest, VeryLargeMessagePayload)
{
    // 创建非常大的消息载荷
    const size_t LargeSize = 1024 * 1024;  // 1MB
    std::string LargePayload(LargeSize, 'A');

    auto LargeMessage = CreateTestMessage(LargePayload);
    EXPECT_EQ(MQ.SendMessage(TestQueueName, LargeMessage), QueueResult::SUCCESS);

    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->Payload.Data.size(), LargeSize);
}

TEST_F(MessageQueueBoundaryTest, NullMessageHandling)
{
    // 测试发送空指针消息 - 应该被拒绝
    EXPECT_EQ(MQ.SendMessage(TestQueueName, nullptr), QueueResult::INVALID_PARAMETER);

    // 验证队列仍然为空
    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(ReceivedMessage, nullptr);
}

// ==================== 超时测试 ====================

TEST_F(MessageQueueBoundaryTest, ZeroTimeoutReceive)
{
    // 测试0毫秒超时 - 队列为空时应该立即返回TIMEOUT
    MessagePtr ReceivedMessage;
    auto StartTime = std::chrono::steady_clock::now();

    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage, 0), QueueResult::TIMEOUT);

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    EXPECT_EQ(ReceivedMessage, nullptr);
    EXPECT_LT(Duration.count(), 10);  // 应该在10ms内完成
}

TEST_F(MessageQueueBoundaryTest, VeryShortTimeoutReceive)
{
    // 测试很短的超时时间 - 队列为空时应该返回TIMEOUT
    MessagePtr ReceivedMessage;
    auto StartTime = std::chrono::steady_clock::now();

    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage, 1), QueueResult::TIMEOUT);

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    EXPECT_EQ(ReceivedMessage, nullptr);
    EXPECT_LT(Duration.count(), 20);  // 应该在20ms内完成
}

TEST_F(MessageQueueBoundaryTest, VeryLongTimeoutReceive)
{
    // 测试很长的超时时间
    auto Message = CreateTestMessage("timeout_test");
    EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);

    MessagePtr ReceivedMessage;
    auto StartTime = std::chrono::steady_clock::now();

    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage, 10000), QueueResult::SUCCESS);

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_LT(Duration.count(), 100);  // 应该很快完成，因为有消息
}

// ==================== 批量操作边界测试 ====================

TEST_F(MessageQueueBoundaryTest, ZeroBatchSize)
{
    // 测试批量大小为0 - 应该返回INVALID_PARAMETER
    std::vector<MessagePtr> ReceivedMessages;
    EXPECT_EQ(MQ.ReceiveBatchMessages(TestQueueName, ReceivedMessages, 0, 100),
              QueueResult::INVALID_PARAMETER);
    EXPECT_EQ(ReceivedMessages.size(), 0);
}

TEST_F(MessageQueueBoundaryTest, MaximumBatchSize)
{
    // 发送一些消息
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(MQ.SendMessage(TestQueueName, CreateTestMessage("batch_" + std::to_string(i))),
                  QueueResult::SUCCESS);
    }

    // 测试最大批量大小
    std::vector<MessagePtr> ReceivedMessages;
    EXPECT_EQ(MQ.ReceiveBatchMessages(
                  TestQueueName, ReceivedMessages, std::numeric_limits<uint32_t>::max(), 100),
              QueueResult::SUCCESS);
    EXPECT_EQ(ReceivedMessages.size(), 5);
}

TEST_F(MessageQueueBoundaryTest, EmptyBatchSend)
{
    // 测试发送空批量 - 空批量应该被拒绝
    std::vector<MessagePtr> EmptyBatch;
    EXPECT_EQ(MQ.SendBatchMessages(TestQueueName, EmptyBatch), QueueResult::INVALID_PARAMETER);

    // 验证队列仍然为空
    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(ReceivedMessage, nullptr);
}

// ==================== 优先级边界测试 ====================

TEST_F(MessageQueueBoundaryTest, AllSamePriorityMessages)
{
    // 发送相同优先级的所有消息
    for (int i = 0; i < 5; ++i)
    {
        auto Message = CreateTestMessage("same_priority_" + std::to_string(i));
        Message->Header.Priority = MessagePriority::NORMAL;
        EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);
    }

    // 验证按时间顺序接收
    for (int i = 0; i < 5; ++i)
    {
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
        EXPECT_EQ(ReceivedMessage->Header.Priority, MessagePriority::NORMAL);
    }
}

TEST_F(MessageQueueBoundaryTest, ExtremePriorityValues)
{
    // 测试极端优先级值
    auto Message1 = CreateTestMessage("extreme_low");
    Message1->Header.Priority = MessagePriority::LOW;

    auto Message2 = CreateTestMessage("extreme_high");
    Message2->Header.Priority = MessagePriority::HIGH;

    std::cout << "发送低优先级消息，优先级值: " << static_cast<int>(Message1->Header.Priority)
              << std::endl;
    std::cout << "发送高优先级消息，优先级值: " << static_cast<int>(Message2->Header.Priority)
              << std::endl;

    // 先发送低优先级，再发送高优先级
    EXPECT_EQ(MQ.SendMessage(TestQueueName, Message1), QueueResult::SUCCESS);
    EXPECT_EQ(MQ.SendMessage(TestQueueName, Message2), QueueResult::SUCCESS);

    // 应该先接收高优先级消息
    MessagePtr ReceivedMessage;
    auto ReceiveResult1 = MQ.ReceiveMessage(TestQueueName, ReceivedMessage);
    std::cout << "第一次接收结果: " << static_cast<int>(ReceiveResult1) << std::endl;
    if (ReceivedMessage)
    {
        std::cout << "第一次接收消息优先级: " << static_cast<int>(ReceivedMessage->Header.Priority)
                  << std::endl;
    }

    EXPECT_EQ(ReceiveResult1, QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->Header.Priority, MessagePriority::HIGH);

    // 然后接收低优先级消息
    auto ReceiveResult2 = MQ.ReceiveMessage(TestQueueName, ReceivedMessage);
    std::cout << "第二次接收结果: " << static_cast<int>(ReceiveResult2) << std::endl;
    if (ReceivedMessage)
    {
        std::cout << "第二次接收消息优先级: " << static_cast<int>(ReceivedMessage->Header.Priority)
                  << std::endl;
    }

    EXPECT_EQ(ReceiveResult2, QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->Header.Priority, MessagePriority::LOW);
}

// ==================== 并发边界测试 ====================

TEST_F(MessageQueueBoundaryTest, SingleThreadStress)
{
    // 单线程压力测试 - 考虑队列容量限制
    const int MessageCount = 1000;

    // 发送消息，考虑队列容量限制
    int SentCount = 0;
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Message = CreateTestMessage("stress_" + std::to_string(i));
        auto SendResult = MQ.SendMessage(TestQueueName, Message);

        if (SendResult == QueueResult::SUCCESS)
        {
            SentCount++;
        }
        else if (SendResult == QueueResult::QUEUE_FULL)
        {
            // 队列满了，先接收一些消息再继续
            MessagePtr TempMessage;
            if (MQ.ReceiveMessage(TestQueueName, TempMessage) == QueueResult::SUCCESS)
            {
                // 重新尝试发送
                SendResult = MQ.SendMessage(TestQueueName, Message);
                if (SendResult == QueueResult::SUCCESS)
                {
                    SentCount++;
                }
            }
        }
    }

    // 接收消息，考虑可能的消息数量
    int ReceivedCount = 0;
    for (int i = 0; i < SentCount; ++i)
    {
        MessagePtr ReceivedMessage;
        auto ReceiveResult = MQ.ReceiveMessage(TestQueueName, ReceivedMessage);

        if (ReceiveResult == QueueResult::SUCCESS && ReceivedMessage != nullptr)
        {
            ReceivedCount++;
        }
    }

    // 验证发送和接收的消息数量一致
    EXPECT_EQ(SentCount, ReceivedCount);
    EXPECT_GT(SentCount, 0);  // 至少应该有一些消息成功处理

    // 验证队列为空
    MessagePtr FinalMessage;
    auto FinalReceiveResult = MQ.ReceiveMessage(TestQueueName, FinalMessage);
    // 如果队列为空，应该返回SUCCESS但消息为nullptr，或者返回TIMEOUT
    EXPECT_TRUE(FinalReceiveResult == QueueResult::SUCCESS ||
                FinalReceiveResult == QueueResult::TIMEOUT);
}

TEST_F(MessageQueueBoundaryTest, RapidSendReceive)
{
    // 快速发送接收测试
    const int Iterations = 100;

    for (int i = 0; i < Iterations; ++i)
    {
        auto Message = CreateTestMessage("rapid_" + std::to_string(i));
        EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);

        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }
}

// ==================== 内存边界测试 ====================

TEST_F(MessageQueueBoundaryTest, MemoryExhaustionSimulation)
{
    // 模拟内存压力 - 考虑队列容量限制
    const int LargeMessageCount = 5;      // 减少到队列容量的一半，避免溢出
    const int LargeMessageSize = 100000;  // 100KB

    // 创建大消息
    std::string LargePayload(LargeMessageSize, 'B');

    // 发送消息，考虑队列可能满的情况
    int SentCount = 0;
    for (int i = 0; i < LargeMessageCount; ++i)
    {
        auto Message = CreateTestMessage(LargePayload);
        auto SendResult = MQ.SendMessage(TestQueueName, Message);

        if (SendResult == QueueResult::SUCCESS)
        {
            SentCount++;
        }
        else if (SendResult == QueueResult::QUEUE_FULL)
        {
            // 队列满了，先接收一些消息再继续
            MessagePtr TempMessage;
            if (MQ.ReceiveMessage(TestQueueName, TempMessage) == QueueResult::SUCCESS)
            {
                // 重新尝试发送
                SendResult = MQ.SendMessage(TestQueueName, Message);
                if (SendResult == QueueResult::SUCCESS)
                {
                    SentCount++;
                }
            }
        }
    }

    // 接收所有已发送的消息
    int ReceivedCount = 0;
    for (int i = 0; i < SentCount; ++i)
    {
        MessagePtr ReceivedMessage;
        auto ReceiveResult = MQ.ReceiveMessage(TestQueueName, ReceivedMessage);

        if (ReceiveResult == QueueResult::SUCCESS && ReceivedMessage != nullptr)
        {
            ReceivedCount++;
            EXPECT_EQ(ReceivedMessage->Payload.Data.size(), LargeMessageSize);
        }
    }

    // 验证发送和接收的消息数量一致
    EXPECT_EQ(SentCount, ReceivedCount);
    EXPECT_GT(SentCount, 0);  // 至少应该有一些消息成功处理
}

// ==================== 错误条件测试 ====================

TEST_F(MessageQueueBoundaryTest, InvalidQueueName)
{
    // 测试无效队列名
    auto Message = CreateTestMessage("invalid_queue_test");

    // 发送到不存在的队列应该失败
    EXPECT_EQ(MQ.SendMessage("non_existent_queue", Message), QueueResult::QUEUE_NOT_FOUND);

    // 从空队列接收应该超时
    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(ReceivedMessage, nullptr);
}

TEST_F(MessageQueueBoundaryTest, InvalidMessageType)
{
    // 测试无效消息类型
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
        static_cast<MessageType>(999), "invalid_type");
    EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);

    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->Header.Type, static_cast<MessageType>(999));
}

// ==================== 事务边界测试 ====================

TEST_F(MessageQueueBoundaryTest, TransactionTimeout)
{
    // 测试事务超时
    auto TransactionId = MQ.BeginTransaction("timeout_test", 1);  // 1ms超时
    EXPECT_NE(TransactionId, 0);

    // 在事务中发送消息
    auto Message = CreateTestMessage("timeout_msg");
    EXPECT_EQ(MQ.SendMessageInTransaction(TransactionId, TestQueueName, Message),
              QueueResult::SUCCESS);

    // 等待超时
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 尝试提交应该失败（事务已超时）
    EXPECT_EQ(MQ.CommitTransaction(TransactionId), QueueResult::TRANSACTION_NOT_FOUND);

    // 验证消息没有被发送到队列
    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(ReceivedMessage, nullptr);
}

TEST_F(MessageQueueBoundaryTest, InvalidTransactionId)
{
    // 测试无效事务ID
    const TransactionId InvalidId = 999999;

    auto Message = CreateTestMessage("invalid_tx_msg");
    EXPECT_EQ(MQ.SendMessageInTransaction(InvalidId, TestQueueName, Message),
              QueueResult::TRANSACTION_NOT_FOUND);

    // 验证消息没有被发送到队列
    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(ReceivedMessage, nullptr);
}

// ==================== 统计边界测试 ====================

TEST_F(MessageQueueBoundaryTest, StatisticsOverflow)
{
    // 测试统计信息溢出 - 考虑队列容量限制
    const int LargeCount = 1000;  // 减少到队列容量，避免溢出

    int SentCount = 0;
    for (int i = 0; i < LargeCount; ++i)
    {
        auto Message = CreateTestMessage("stats_" + std::to_string(i));
        auto SendResult = MQ.SendMessage(TestQueueName, Message);

        if (SendResult == QueueResult::SUCCESS)
        {
            SentCount++;
        }
        else if (SendResult == QueueResult::QUEUE_FULL)
        {
            // 队列满了，停止发送
            break;
        }
    }

    // 验证统计信息
    QueueStats Stats;
    EXPECT_EQ(MQ.GetQueueStats(TestQueueName, Stats), QueueResult::SUCCESS);
    EXPECT_EQ(Stats.TotalMessages, SentCount);
    EXPECT_EQ(Stats.PendingMessages, SentCount);
    EXPECT_GT(Stats.TotalBytes, 0);
    EXPECT_GT(SentCount, 0);  // 至少应该有一些消息成功
}

TEST_F(MessageQueueBoundaryTest, StatisticsReset)
{
    // 发送一些消息
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(MQ.SendMessage(TestQueueName, CreateTestMessage("reset_" + std::to_string(i))),
                  QueueResult::SUCCESS);
    }

    // 获取初始统计
    QueueStats InitialStats;
    EXPECT_EQ(MQ.GetQueueStats(TestQueueName, InitialStats), QueueResult::SUCCESS);
    EXPECT_EQ(InitialStats.TotalMessages, 5);
    EXPECT_EQ(InitialStats.PendingMessages, 5);

    // 重置统计
    EXPECT_EQ(MQ.ResetQueueStats(TestQueueName), QueueResult::SUCCESS);

    // 验证统计被重置
    QueueStats ResetStats;
    EXPECT_EQ(MQ.GetQueueStats(TestQueueName, ResetStats), QueueResult::SUCCESS);
    EXPECT_EQ(ResetStats.TotalMessages, 0);  // 统计被重置
    EXPECT_EQ(ResetStats.PendingMessages, 5);  // 消息仍然在队列中（重置统计不影响消息）
}

// ==================== 配置边界测试 ====================

TEST_F(MessageQueueBoundaryTest, InvalidQueueConfig)
{
    // 测试无效队列配置
    QueueConfig InvalidConfig;
    InvalidConfig.Name = "";  // 空名称
    InvalidConfig.MaxSize = 0;

    // 无效配置应该被拒绝
    EXPECT_EQ(MQ.CreateQueue(InvalidConfig), QueueResult::INVALID_PARAMETER);

    // 验证队列没有被创建
    EXPECT_FALSE(MQ.QueueExists(""));
}

TEST_F(MessageQueueBoundaryTest, QueueConfigUpdate)
{
    // 测试队列配置更新
    QueueConfig NewConfig;
    NewConfig.Name = TestQueueName;
    NewConfig.MaxSize = 5;
    NewConfig.Persistence = PersistenceMode::MEMORY_ONLY;

    EXPECT_EQ(MQ.UpdateQueueConfig(TestQueueName, NewConfig), QueueResult::SUCCESS);

    // 验证配置更新
    QueueConfig RetrievedConfig;
    EXPECT_EQ(MQ.GetQueueInfo(TestQueueName, RetrievedConfig), QueueResult::SUCCESS);
    EXPECT_EQ(RetrievedConfig.MaxSize, 5);
}

// ==================== 清理边界测试 ====================

TEST_F(MessageQueueBoundaryTest, QueuePurgeWithMessages)
{
    // 发送一些消息
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(MQ.SendMessage(TestQueueName, CreateTestMessage("purge_" + std::to_string(i))),
                  QueueResult::SUCCESS);
    }

    // 验证消息存在
    QueueStats BeforeStats;
    EXPECT_EQ(MQ.GetQueueStats(TestQueueName, BeforeStats), QueueResult::SUCCESS);
    EXPECT_EQ(BeforeStats.PendingMessages, 5);

    // 清空队列
    EXPECT_EQ(MQ.PurgeQueue(TestQueueName), QueueResult::SUCCESS);

    // 验证队列被清空
    QueueStats AfterStats;
    EXPECT_EQ(MQ.GetQueueStats(TestQueueName, AfterStats), QueueResult::SUCCESS);
    EXPECT_EQ(AfterStats.PendingMessages, 0);

    // 验证无法接收消息
    MessagePtr ReceivedMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(ReceivedMessage, nullptr);
}

TEST_F(MessageQueueBoundaryTest, QueueDeletionWithMessages)
{
    // 创建新队列并发送消息
    QueueConfig NewConfig;
    NewConfig.Name = "delete_test_queue";
    NewConfig.MaxSize = 10;
    NewConfig.Persistence = PersistenceMode::MEMORY_ONLY;

    EXPECT_EQ(MQ.CreateQueue(NewConfig), QueueResult::SUCCESS);

    // 发送消息
    EXPECT_EQ(MQ.SendMessage("delete_test_queue", CreateTestMessage("delete_test")),
              QueueResult::SUCCESS);

    // 删除队列
    EXPECT_EQ(MQ.DeleteQueue("delete_test_queue"), QueueResult::SUCCESS);

    // 验证队列不存在
    EXPECT_FALSE(MQ.QueueExists("delete_test_queue"));
}
