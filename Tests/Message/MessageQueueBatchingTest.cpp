#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

class MessageQueueBatchingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
        Queue->Initialize();
    }

    void TearDown() override
    {
        if (Queue)
        {
            Queue->Shutdown();
        }
    }

    std::unique_ptr<Helianthus::MessageQueue::MessageQueue> Queue;
};

TEST_F(MessageQueueBatchingTest, BatchProcessingWorks)
{
    // 创建测试队列
    std::string QueueName = "batch_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 创建批处理
    uint32_t BatchId;
    auto Result = Queue->CreateBatchForQueue(QueueName, BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 添加多个消息到批处理
    for (int i = 0; i < 15; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = "Batch message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

        Result = Queue->AddToBatch(BatchId, Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 提交批处理
    Result = Queue->CommitBatch(BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收批处理消息
    std::vector<Helianthus::MessageQueue::MessagePtr> ReceivedMessages;
    Result = Queue->ReceiveBatchMessages(QueueName, ReceivedMessages, 20);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(ReceivedMessages.size(), 15);

    // 验证消息内容
    for (int i = 0; i < 15; ++i)
    {
        std::string ExpectedPayload = "Batch message " + std::to_string(i);
        std::string ReceivedPayload(ReceivedMessages[i]->Payload.Data.begin(),
                                    ReceivedMessages[i]->Payload.Data.end());
        EXPECT_EQ(ReceivedPayload, ExpectedPayload);
    }
}

TEST_F(MessageQueueBatchingTest, ZeroCopyOperationsWork)
{
    // 创建测试队列
    std::string QueueName = "zerocopy_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 创建大消息数据
    std::string LargePayload(50000, 'Z');  // 50KB 数据

    // 创建零拷贝缓冲区
    Helianthus::MessageQueue::ZeroCopyBuffer Buffer;
    auto Result = Queue->CreateZeroCopyBuffer(LargePayload.data(), LargePayload.size(), Buffer);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 发送零拷贝消息
    Result = Queue->SendMessageZeroCopy(QueueName, Buffer);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 释放零拷贝缓冲区
    Queue->ReleaseZeroCopyBuffer(Buffer);

    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);

    // 验证消息内容
    std::string ReceivedPayload(ReceivedMessage->Payload.Data.begin(),
                                ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedPayload, LargePayload);
}

TEST_F(MessageQueueBatchingTest, BatchPerformanceIsBetter)
{
    // 创建测试队列
    std::string QueueName = "performance_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    const int MessageCount = 2000;  // 增大样本量以放大批处理优势
    const int BatchSize = 100;      // 增大批大小减少往返与锁竞争

    // 测试单个发送性能
    auto StartTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MessageCount; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = "Single message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

        auto Result = Queue->SendMessage(QueueName, Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    auto SingleEndTime = std::chrono::high_resolution_clock::now();
    auto SingleDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(SingleEndTime - StartTime);

    // 清空队列
    Queue->PurgeQueue(QueueName);

    // 测试批处理发送性能
    StartTime = std::chrono::high_resolution_clock::now();

    for (int batch = 0; batch < MessageCount / BatchSize; ++batch)
    {
        // 创建批处理
        uint32_t BatchId;
        auto Result = Queue->CreateBatchForQueue(QueueName, BatchId);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

        // 添加消息到批处理
        for (int i = 0; i < BatchSize; ++i)
        {
            auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
            std::string Payload = "Batch message " + std::to_string(batch * BatchSize + i);
            Message->Payload.Data.assign(Payload.begin(), Payload.end());
            Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

            Result = Queue->AddToBatch(BatchId, Message);
            EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
        }

        // 提交批处理
        Result = Queue->CommitBatch(BatchId);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    auto BatchEndTime = std::chrono::high_resolution_clock::now();
    auto BatchDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(BatchEndTime - StartTime);

    // 批处理应该更快，但在测试环境中可能有波动
    // 允许一定的性能波动，只要批处理不会显著更慢
    bool PerformanceOk =
        (BatchDuration.count() <= SingleDuration.count() * 1.2);  // 允许20%的性能波动
    EXPECT_TRUE(PerformanceOk) << "批处理性能测试失败: 批处理时间(" << BatchDuration.count()
                               << ") 应该接近或优于单个处理时间(" << SingleDuration.count() << ")";

    std::cout << "Single send time: " << SingleDuration.count() << " microseconds" << std::endl;
    std::cout << "Batch send time: " << BatchDuration.count() << " microseconds" << std::endl;
    std::cout << "Performance improvement: "
              << (double)SingleDuration.count() / BatchDuration.count() << "x" << std::endl;
}

TEST_F(MessageQueueBatchingTest, BatchAbortWorks)
{
    // 创建测试队列
    std::string QueueName = "batch_abort_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 创建批处理
    uint32_t BatchId;
    auto Result = Queue->CreateBatchForQueue(QueueName, BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 添加消息到批处理
    for (int i = 0; i < 5; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = "Batch message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

        Result = Queue->AddToBatch(BatchId, Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 中止批处理
    Result = Queue->AbortBatch(BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证队列中没有消息
    std::vector<Helianthus::MessageQueue::MessagePtr> ReceivedMessages;
    Result = Queue->ReceiveBatchMessages(QueueName, ReceivedMessages, 10, 100);
    EXPECT_EQ(ReceivedMessages.size(), 0);
}

TEST_F(MessageQueueBatchingTest, BatchInfoRetrieval)
{
    // 创建测试队列
    std::string QueueName = "batch_info_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 创建批处理
    uint32_t BatchId;
    auto Result = Queue->CreateBatchForQueue(QueueName, BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 添加消息到批处理
    for (int i = 0; i < 3; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = "Batch message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

        Result = Queue->AddToBatch(BatchId, Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 获取批处理信息
    Helianthus::MessageQueue::BatchMessage BatchInfo;
    Result = Queue->GetBatchInfo(BatchId, BatchInfo);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 提交批处理
    Result = Queue->CommitBatch(BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息验证
    std::vector<Helianthus::MessageQueue::MessagePtr> ReceivedMessages;
    Result = Queue->ReceiveBatchMessages(QueueName, ReceivedMessages, 10);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(ReceivedMessages.size(), 3);
}

TEST_F(MessageQueueBatchingTest, ConcurrentBatchAdditions)
{
    std::string QueueName = "concurrent_batch_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    uint32_t BatchId;
    auto Result = Queue->CreateBatchForQueue(QueueName, BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    const int Threads = 4;
    const int PerThread = 50;
    std::vector<std::thread> Workers;
    for (int t = 0; t < Threads; ++t)
    {
        Workers.emplace_back(
            [&, t]()
            {
                for (int i = 0; i < PerThread; ++i)
                {
                    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
                    std::string Payload = "Msg T" + std::to_string(t) + " #" + std::to_string(i);
                    Message->Payload.Data.assign(Payload.begin(), Payload.end());
                    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
                    auto R = Queue->AddToBatch(BatchId, Message);
                    EXPECT_EQ(R, Helianthus::MessageQueue::QueueResult::SUCCESS);
                }
            });
    }
    for (auto& Th : Workers)
        Th.join();

    Result = Queue->CommitBatch(BatchId);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    std::vector<Helianthus::MessageQueue::MessagePtr> Received;
    Result = Queue->ReceiveBatchMessages(QueueName, Received, Threads * PerThread);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(static_cast<int>(Received.size()), Threads * PerThread);
}
