#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Helianthus::MessageQueue;

class MessageQueueMemoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MQ.Initialize();

        // 创建测试队列
        QueueConfig Config;
        Config.Name = "memory_test_queue";
        Config.MaxSize = 1000;
        Config.Persistence = PersistenceMode::MEMORY_ONLY;

        ASSERT_EQ(MQ.CreateQueue(Config), QueueResult::SUCCESS);
    }

    void TearDown() override
    {
        MQ.Shutdown();
    }

    MessagePtr CreateTestMessage(const std::string& Payload = "test_payload")
    {
        auto Message =
            std::make_shared<Helianthus::MessageQueue::Message>(MessageType::TEXT, Payload);
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        return Message;
    }

    MessageQueue MQ;
    const std::string TestQueueName = "memory_test_queue";
};

// ==================== 内存分配测试 ====================

TEST_F(MessageQueueMemoryTest, MessageAllocationDeallocation)
{
    const int MessageCount = 1000;
    std::vector<MessagePtr> Messages;

    // 分配大量消息
    for (int i = 0; i < MessageCount; ++i)
    {
        Messages.push_back(CreateTestMessage("alloc_test_" + std::to_string(i)));
    }

    // 发送所有消息
    for (const auto& Message : Messages)
    {
        EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);
    }

    // 接收所有消息
    for (int i = 0; i < MessageCount; ++i)
    {
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }

    // 清空消息向量，触发析构
    Messages.clear();

    // 验证队列为空
    MessagePtr FinalMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(FinalMessage, nullptr);
}

TEST_F(MessageQueueMemoryTest, LargeMessageMemoryHandling)
{
    const int LargeMessageCount = 100;
    const int LargeMessageSize = 100000;  // 100KB

    // 创建大消息
    std::string LargePayload(LargeMessageSize, 'L');

    for (int i = 0; i < LargeMessageCount; ++i)
    {
        auto Message = CreateTestMessage(LargePayload);
        EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);
    }

    // 接收所有大消息
    for (int i = 0; i < LargeMessageCount; ++i)
    {
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
        EXPECT_EQ(ReceivedMessage->Payload.Data.size(), LargeMessageSize);
    }

    // 验证队列为空
    MessagePtr FinalMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(FinalMessage, nullptr);
}

// ==================== 循环内存测试 ====================

TEST_F(MessageQueueMemoryTest, CircularMemoryUsage)
{
    const int Cycles = 50;
    const int MessagesPerCycle = 100;

    for (int cycle = 0; cycle < Cycles; ++cycle)
    {
        // 发送消息
        for (int i = 0; i < MessagesPerCycle; ++i)
        {
            auto Message =
                CreateTestMessage("cycle_" + std::to_string(cycle) + "_msg_" + std::to_string(i));
            EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);
        }

        // 接收消息
        for (int i = 0; i < MessagesPerCycle; ++i)
        {
            MessagePtr ReceivedMessage;
            EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
            EXPECT_NE(ReceivedMessage, nullptr);
        }

        // 验证队列为空
        MessagePtr FinalMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
        EXPECT_EQ(FinalMessage, nullptr);
    }
}

TEST_F(MessageQueueMemoryTest, InterleavedMemoryOperations)
{
    const int Operations = 1000;

    for (int i = 0; i < Operations; ++i)
    {
        // 发送消息
        auto Message = CreateTestMessage("interleaved_" + std::to_string(i));
        EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);

        // 立即接收消息
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);

        // 验证消息内容
        std::string ExpectedPayload = "interleaved_" + std::to_string(i);
        std::string ActualPayload(ReceivedMessage->Payload.Data.begin(),
                                  ReceivedMessage->Payload.Data.end());
        EXPECT_EQ(ActualPayload, ExpectedPayload);
    }
}

// ==================== 批量内存测试 ====================

TEST_F(MessageQueueMemoryTest, BatchMemoryOperations)
{
    const int BatchCount = 20;
    const int MessagesPerBatch = 50;

    for (int batch = 0; batch < BatchCount; ++batch)
    {
        // 批量发送
        std::vector<MessagePtr> BatchMessages;
        for (int i = 0; i < MessagesPerBatch; ++i)
        {
            BatchMessages.push_back(
                CreateTestMessage("batch_" + std::to_string(batch) + "_msg_" + std::to_string(i)));
        }

        EXPECT_EQ(MQ.SendBatchMessages(TestQueueName, BatchMessages), QueueResult::SUCCESS);

        // 批量接收
        std::vector<MessagePtr> ReceivedBatch;
        EXPECT_EQ(MQ.ReceiveBatchMessages(TestQueueName, ReceivedBatch, MessagesPerBatch, 1000),
                  QueueResult::SUCCESS);
        EXPECT_EQ(ReceivedBatch.size(), MessagesPerBatch);

        // 验证消息内容
        for (size_t i = 0; i < ReceivedBatch.size(); ++i)
        {
            std::string ExpectedPayload =
                "batch_" + std::to_string(batch) + "_msg_" + std::to_string(i);
            std::string ActualPayload(ReceivedBatch[i]->Payload.Data.begin(),
                                      ReceivedBatch[i]->Payload.Data.end());
            EXPECT_EQ(ActualPayload, ExpectedPayload);
        }

        // 清空批量向量
        BatchMessages.clear();
        ReceivedBatch.clear();
    }
}

// ==================== 并发内存测试 ====================

TEST_F(MessageQueueMemoryTest, ConcurrentMemoryAccess)
{
    const int ThreadCount = 4;
    const int MessagesPerThread = 100;
    std::atomic<int> SendCount = 0;
    std::atomic<int> ReceiveCount = 0;

    // 启动发送线程
    std::vector<std::thread> SendThreads;
    for (int i = 0; i < ThreadCount; ++i)
    {
        SendThreads.emplace_back(
            [&, i]()
            {
                for (int j = 0; j < MessagesPerThread; ++j)
                {
                    auto Message = CreateTestMessage("concurrent_send_" + std::to_string(i) + "_" +
                                                     std::to_string(j));
                    if (MQ.SendMessage(TestQueueName, Message) == QueueResult::SUCCESS)
                    {
                        SendCount++;
                    }
                }
            });
    }

    // 启动接收线程
    std::vector<std::thread> ReceiveThreads;
    for (int i = 0; i < ThreadCount; ++i)
    {
        ReceiveThreads.emplace_back(
            [&]()
            {
                for (int j = 0; j < MessagesPerThread; ++j)
                {
                    MessagePtr ReceivedMessage;
                    if (MQ.ReceiveMessage(TestQueueName, ReceivedMessage) == QueueResult::SUCCESS &&
                        ReceivedMessage != nullptr)
                    {
                        ReceiveCount++;
                    }
                }
            });
    }

    // 等待所有线程完成
    for (auto& Thread : SendThreads)
    {
        Thread.join();
    }
    for (auto& Thread : ReceiveThreads)
    {
        Thread.join();
    }

    // 验证结果
    EXPECT_EQ(SendCount.load(), ThreadCount * MessagesPerThread);
    EXPECT_EQ(ReceiveCount.load(), ThreadCount * MessagesPerThread);

    // 验证队列为空
    MessagePtr FinalMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(FinalMessage, nullptr);
}

// ==================== 内存压力测试 ====================

TEST_F(MessageQueueMemoryTest, MemoryStressTest)
{
    const int StressCycles = 10;
    const int MessagesPerCycle = 500;

    for (int cycle = 0; cycle < StressCycles; ++cycle)
    {
        // 创建大量消息
        std::vector<MessagePtr> Messages;
        for (int i = 0; i < MessagesPerCycle; ++i)
        {
            Messages.push_back(
                CreateTestMessage("stress_" + std::to_string(cycle) + "_" + std::to_string(i)));
        }

        // 批量发送
        EXPECT_EQ(MQ.SendBatchMessages(TestQueueName, Messages), QueueResult::SUCCESS);

        // 批量接收
        std::vector<MessagePtr> ReceivedMessages;
        EXPECT_EQ(MQ.ReceiveBatchMessages(TestQueueName, ReceivedMessages, MessagesPerCycle, 5000),
                  QueueResult::SUCCESS);
        EXPECT_EQ(ReceivedMessages.size(), MessagesPerCycle);

        // 清空向量
        Messages.clear();
        ReceivedMessages.clear();

        // 验证队列为空
        MessagePtr FinalMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
        EXPECT_EQ(FinalMessage, nullptr);
    }
}

// ==================== 队列容量内存测试 ====================

TEST_F(MessageQueueMemoryTest, QueueCapacityMemoryTest)
{
    // 创建小容量队列
    QueueConfig SmallConfig;
    SmallConfig.Name = "small_memory_queue";
    SmallConfig.MaxSize = 10;
    SmallConfig.Persistence = PersistenceMode::MEMORY_ONLY;

    ASSERT_EQ(MQ.CreateQueue(SmallConfig), QueueResult::SUCCESS);

    // 发送消息直到队列满
    for (int i = 0; i < 15; ++i)
    {
        auto Message = CreateTestMessage("capacity_test_" + std::to_string(i));
        EXPECT_EQ(MQ.SendMessage("small_memory_queue", Message), QueueResult::SUCCESS);
    }

    // 接收所有消息
    for (int i = 0; i < 15; ++i)
    {
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage("small_memory_queue", ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }

    // 验证队列为空
    MessagePtr FinalMessage;
    EXPECT_EQ(MQ.ReceiveMessage("small_memory_queue", FinalMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(FinalMessage, nullptr);
}

// ==================== 事务内存测试 ====================

TEST_F(MessageQueueMemoryTest, TransactionMemoryTest)
{
    const int TransactionCount = 100;

    for (int i = 0; i < TransactionCount; ++i)
    {
        // 开始事务
        auto TransactionId = MQ.BeginTransaction("memory_tx_" + std::to_string(i), 5000);
        EXPECT_NE(TransactionId, 0);

        // 在事务中发送消息
        auto Message = CreateTestMessage("tx_memory_" + std::to_string(i));
        EXPECT_EQ(MQ.SendMessageInTransaction(TransactionId, TestQueueName, Message),
                  QueueResult::SUCCESS);

        // 提交事务
        EXPECT_EQ(MQ.CommitTransaction(TransactionId), QueueResult::SUCCESS);

        // 接收消息
        MessagePtr ReceivedMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }

    // 验证队列为空
    MessagePtr FinalMessage;
    EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
    EXPECT_EQ(FinalMessage, nullptr);
}

// ==================== 消费者内存测试 ====================

TEST_F(MessageQueueMemoryTest, ConsumerMemoryTest)
{
    // 注册消费者
    ConsumerConfig ConsumerConfig;
    ConsumerConfig.ConsumerId = "memory_consumer";
    ConsumerConfig.BatchSize = 10;
    ConsumerConfig.BatchTimeoutMs = 1000;

    std::atomic<int> MessageCount = 0;
    MessageHandler Handler = [&](MessagePtr Message)
    {
        MessageCount++;
        return true;
    };

    EXPECT_EQ(MQ.RegisterConsumer(TestQueueName, ConsumerConfig, Handler), QueueResult::SUCCESS);

    // 发送消息
    const int MessageCountToSend = 50;
    for (int i = 0; i < MessageCountToSend; ++i)
    {
        EXPECT_EQ(MQ.SendMessage(TestQueueName,
                                 CreateTestMessage("consumer_memory_" + std::to_string(i))),
                  QueueResult::SUCCESS);
    }

    // 等待消费者处理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 验证消息被处理
    EXPECT_EQ(MessageCount.load(), MessageCountToSend);

    // 注销消费者
    EXPECT_EQ(MQ.UnregisterConsumer(TestQueueName, "memory_consumer"), QueueResult::SUCCESS);
}

// ==================== 多队列内存测试 ====================

TEST_F(MessageQueueMemoryTest, MultiQueueMemoryTest)
{
    const int QueueCount = 5;
    const int MessagesPerQueue = 20;

    // 创建多个队列
    std::vector<std::string> QueueNames;
    for (int i = 0; i < QueueCount; ++i)
    {
        std::string QueueName = "multi_queue_" + std::to_string(i);
        QueueConfig Config;
        Config.Name = QueueName;
        Config.MaxSize = 100;
        Config.Persistence = PersistenceMode::MEMORY_ONLY;

        EXPECT_EQ(MQ.CreateQueue(Config), QueueResult::SUCCESS);
        QueueNames.push_back(QueueName);
    }

    // 向每个队列发送消息
    for (const auto& QueueName : QueueNames)
    {
        for (int i = 0; i < MessagesPerQueue; ++i)
        {
            auto Message = CreateTestMessage("multi_" + QueueName + "_" + std::to_string(i));
            EXPECT_EQ(MQ.SendMessage(QueueName, Message), QueueResult::SUCCESS);
        }
    }

    // 从每个队列接收消息
    for (const auto& QueueName : QueueNames)
    {
        for (int i = 0; i < MessagesPerQueue; ++i)
        {
            MessagePtr ReceivedMessage;
            EXPECT_EQ(MQ.ReceiveMessage(QueueName, ReceivedMessage), QueueResult::SUCCESS);
            EXPECT_NE(ReceivedMessage, nullptr);
        }

        // 验证队列为空
        MessagePtr FinalMessage;
        EXPECT_EQ(MQ.ReceiveMessage(QueueName, FinalMessage), QueueResult::TIMEOUT);
        EXPECT_EQ(FinalMessage, nullptr);
    }

    // 删除所有队列
    for (const auto& QueueName : QueueNames)
    {
        EXPECT_EQ(MQ.DeleteQueue(QueueName), QueueResult::SUCCESS);
    }
}

// ==================== 长时间运行内存测试 ====================

TEST_F(MessageQueueMemoryTest, LongRunningMemoryTest)
{
    const int LongRunningCycles = 20;
    const int MessagesPerCycle = 100;

    for (int cycle = 0; cycle < LongRunningCycles; ++cycle)
    {
        // 发送消息
        for (int i = 0; i < MessagesPerCycle; ++i)
        {
            auto Message =
                CreateTestMessage("long_run_" + std::to_string(cycle) + "_" + std::to_string(i));
            EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);
        }

        // 接收消息
        for (int i = 0; i < MessagesPerCycle; ++i)
        {
            MessagePtr ReceivedMessage;
            EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, ReceivedMessage), QueueResult::SUCCESS);
            EXPECT_NE(ReceivedMessage, nullptr);
        }

        // 验证队列为空
        MessagePtr FinalMessage;
        EXPECT_EQ(MQ.ReceiveMessage(TestQueueName, FinalMessage), QueueResult::TIMEOUT);
        EXPECT_EQ(FinalMessage, nullptr);

        // 短暂休息
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ==================== 随机内存测试 ====================

TEST_F(MessageQueueMemoryTest, RandomMemoryOperations)
{
    const int RandomOperations = 1000;
    std::random_device Rd;
    std::mt19937 Gen(Rd());
    std::uniform_int_distribution<> Dis(1, 10);

    for (int i = 0; i < RandomOperations; ++i)
    {
        int Operation = Dis(Gen);

        switch (Operation)
        {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                // 发送消息
                {
                    auto Message = CreateTestMessage("random_send_" + std::to_string(i));
                    EXPECT_EQ(MQ.SendMessage(TestQueueName, Message), QueueResult::SUCCESS);
                }
                break;

            case 6:
            case 7:
            case 8:
                // 接收消息
                {
                    MessagePtr ReceivedMessage;
                    MQ.ReceiveMessage(TestQueueName, ReceivedMessage);
                    // 不验证结果，因为队列可能为空
                }
                break;

            case 9:
                // 批量发送
                {
                    std::vector<MessagePtr> Batch;
                    int BatchSize = Dis(Gen);
                    for (int j = 0; j < BatchSize; ++j)
                    {
                        Batch.push_back(CreateTestMessage("random_batch_" + std::to_string(i) +
                                                          "_" + std::to_string(j)));
                    }
                    MQ.SendBatchMessages(TestQueueName, Batch);
                }
                break;

            case 10:
                // 批量接收
                {
                    std::vector<MessagePtr> ReceivedBatch;
                    MQ.ReceiveBatchMessages(TestQueueName, ReceivedBatch, Dis(Gen), 100);
                }
                break;
        }
    }

    // 清理所有消息
    MessagePtr CleanupMessage;
    while (MQ.ReceiveMessage(TestQueueName, CleanupMessage) == QueueResult::SUCCESS &&
           CleanupMessage != nullptr)
    {
        // 继续接收直到队列为空
    }
}
