#include "Shared/MessageQueue/MessageQueue.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

class SimpleIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>("simple_integration_test",
                                                                         100);
    }

    void TearDown() override
    {
        // 清理队列
        Queue->Clear();
    }

    std::unique_ptr<Helianthus::MessageQueue::MessageQueue> Queue;
};

TEST_F(SimpleIntegrationTest, BasicMessageFlow)
{
    // 基础消息流测试
    auto Message = Helianthus::MessageQueue::Message::Create("test", "Hello Integration Test");

    // 发送消息
    auto SendResult = Queue->Enqueue(Message);
    EXPECT_EQ(SendResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    auto ReceivedMessage = Queue->Dequeue();
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->GetPayload(), "Hello Integration Test");
}

TEST_F(SimpleIntegrationTest, TransactionFlow)
{
    // 事务流测试
    auto Transaction = Queue->BeginTransaction();
    EXPECT_NE(Transaction, nullptr);

    // 在事务中发送消息
    auto Message = Helianthus::MessageQueue::Message::Create("test", "Transaction Message");
    auto SendResult = Transaction->SendMessage(Message);
    EXPECT_EQ(SendResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 提交事务
    auto CommitResult = Queue->CommitTransaction(Transaction);
    EXPECT_EQ(CommitResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证消息已提交
    auto ReceivedMessage = Queue->Dequeue();
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->GetPayload(), "Transaction Message");
}

TEST_F(SimpleIntegrationTest, BatchOperations)
{
    // 批处理测试
    const int BatchSize = 5;

    // 创建批处理
    auto Batch = Queue->CreateBatch();
    EXPECT_NE(Batch, nullptr);

    // 添加消息到批处理
    for (int i = 0; i < BatchSize; ++i)
    {
        auto Message =
            Helianthus::MessageQueue::Message::Create("test", "Batch Message " + std::to_string(i));
        auto AddResult = Batch->AddMessage(Message);
        EXPECT_EQ(AddResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 提交批处理
    auto CommitResult = Queue->CommitBatch(Batch);
    EXPECT_EQ(CommitResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证所有消息都已提交
    for (int i = 0; i < BatchSize; ++i)
    {
        auto ReceivedMessage = Queue->Dequeue();
        EXPECT_NE(ReceivedMessage, nullptr);
        EXPECT_EQ(ReceivedMessage->GetPayload(), "Batch Message " + std::to_string(i));
    }
}

TEST_F(SimpleIntegrationTest, ZeroCopyOperations)
{
    // 零拷贝操作测试
    const std::string TestData = "Zero Copy Test Data";
    const size_t DataSize = TestData.size();

    // 创建零拷贝缓冲区
    auto Buffer = Queue->CreateZeroCopyBuffer(DataSize);
    EXPECT_NE(Buffer, nullptr);

    // 写入数据
    std::memcpy(Buffer->GetData(), TestData.data(), DataSize);

    // 发送零拷贝消息
    auto SendResult = Queue->SendZeroCopy(Buffer);
    EXPECT_EQ(SendResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    auto ReceivedMessage = Queue->Dequeue();
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(ReceivedMessage->GetPayload(), TestData);
}

TEST_F(SimpleIntegrationTest, PerformanceStats)
{
    // 性能统计测试
    const int MessageCount = 10;

    // 发送一些消息
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Message =
            Helianthus::MessageQueue::Message::Create("test", "Stats Message " + std::to_string(i));
        Queue->Enqueue(Message);
    }

    // 接收消息
    for (int i = 0; i < MessageCount; ++i)
    {
        Queue->Dequeue();
    }

    // 检查性能统计
    auto Stats = Queue->GetPerformanceStats();
    EXPECT_GT(Stats.BatchOperations, 0u);
    EXPECT_GT(Stats.ZeroCopyOperations, 0u);
}

TEST_F(SimpleIntegrationTest, ErrorHandling)
{
    // 错误处理测试
    auto Transaction = Queue->BeginTransaction();
    EXPECT_NE(Transaction, nullptr);

    // 尝试发送无效消息
    auto InvalidMessage = std::make_shared<Helianthus::MessageQueue::Message>();
    auto SendResult = Transaction->SendMessage(InvalidMessage);
    EXPECT_NE(SendResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 回滚事务
    auto RollbackResult = Queue->RollbackTransaction(Transaction);
    EXPECT_EQ(RollbackResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证队列为空
    auto ReceivedMessage = Queue->Dequeue();
    EXPECT_EQ(ReceivedMessage, nullptr);
}

TEST_F(SimpleIntegrationTest, ConcurrentAccess)
{
    // 并发访问测试
    const int ThreadCount = 4;
    const int MessagesPerThread = 5;
    std::vector<std::thread> Threads;
    std::atomic<int> SuccessCount{0};

    // 启动发送线程
    for (int t = 0; t < ThreadCount; ++t)
    {
        Threads.emplace_back(
            [this, t, MessagesPerThread, &SuccessCount]()
            {
                for (int i = 0; i < MessagesPerThread; ++i)
                {
                    auto Message = Helianthus::MessageQueue::Message::Create(
                        "test", "Thread " + std::to_string(t) + " Message " + std::to_string(i));
                    auto Result = Queue->Enqueue(Message);
                    if (Result == Helianthus::MessageQueue::QueueResult::SUCCESS)
                    {
                        SuccessCount++;
                    }
                }
            });
    }

    // 等待所有线程完成
    for (auto& Thread : Threads)
    {
        Thread.join();
    }

    // 验证所有消息都发送成功
    EXPECT_EQ(SuccessCount.load(), ThreadCount * MessagesPerThread);

    // 接收所有消息
    int ReceivedCount = 0;
    while (Queue->Dequeue() != nullptr)
    {
        ReceivedCount++;
    }

    EXPECT_EQ(ReceivedCount, ThreadCount * MessagesPerThread);
}
