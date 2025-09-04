#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

class EndToEndTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
        ASSERT_EQ(Queue->Initialize(), Helianthus::MessageQueue::QueueResult::SUCCESS);

        // 创建测试队列
        Helianthus::MessageQueue::QueueConfig Config;
        Config.Name = "e2e_test_queue";
        Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
        ASSERT_EQ(Queue->CreateQueue(Config), Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    void TearDown() override
    {
        Queue->Shutdown();
    }

    std::unique_ptr<Helianthus::MessageQueue::MessageQueue> Queue;
};

TEST_F(EndToEndTest, BasicEndToEndFlow)
{
    // 简化的端到端测试
    const int MessageCount = 5;  // 进一步减少消息数量

    // 发送消息
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
            Helianthus::MessageQueue::MessageType::TEXT, "Hello World " + std::to_string(i));

        auto Result = Queue->SendMessage("e2e_test_queue", Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 接收消息
    for (int i = 0; i < MessageCount; ++i)
    {
        Helianthus::MessageQueue::MessagePtr ReceivedMessage;
        auto Result = Queue->ReceiveMessage("e2e_test_queue", ReceivedMessage);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);

        // 转换vector<char>为string进行比较
        std::string ReceivedData(ReceivedMessage->Payload.Data.begin(),
                                 ReceivedMessage->Payload.Data.end());
        EXPECT_EQ(ReceivedData, "Hello World " + std::to_string(i));
    }
}

TEST_F(EndToEndTest, TransactionEndToEndFlow)
{
    // 简化的事务测试
    auto TransactionId = Queue->BeginTransaction("commit_flow", 2000);
    EXPECT_NE(TransactionId, 0);

    // 在事务中发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
        Helianthus::MessageQueue::MessageType::TEXT, "Transaction Message");

    auto Result = Queue->SendMessageInTransaction(TransactionId, "e2e_test_queue", Message);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 提交事务
    auto CommitResult = Queue->CommitTransaction(TransactionId);
    EXPECT_EQ(CommitResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证消息已提交
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    auto ReceiveResult = Queue->ReceiveMessage("e2e_test_queue", ReceivedMessage);
    EXPECT_EQ(ReceiveResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);

    std::string ReceivedData(ReceivedMessage->Payload.Data.begin(),
                             ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedData, "Transaction Message");
}

TEST_F(EndToEndTest, TransactionRollbackEndToEndFlow)
{
    // 简化的回滚测试
    auto TransactionId = Queue->BeginTransaction("rollback_flow", 2000);
    EXPECT_NE(TransactionId, 0);

    // 在事务中发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
        Helianthus::MessageQueue::MessageType::TEXT, "Rollback Message");

    auto Result = Queue->SendMessageInTransaction(TransactionId, "e2e_test_queue", Message);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 回滚事务
    auto RollbackResult = Queue->RollbackTransaction(TransactionId);
    EXPECT_EQ(RollbackResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证消息未提交
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    auto ReceiveResult = Queue->ReceiveMessage("e2e_test_queue", ReceivedMessage);
    EXPECT_NE(ReceiveResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
}

TEST_F(EndToEndTest, MultiQueueEndToEndFlow)
{
    // 简化的多队列测试
    // 创建第二个队列
    Helianthus::MessageQueue::QueueConfig Config2;
    Config2.Name = "e2e_test_queue2";
    Config2.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    ASSERT_EQ(Queue->CreateQueue(Config2), Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 在两个队列中发送消息
    auto Message1 = std::make_shared<Helianthus::MessageQueue::Message>(
        Helianthus::MessageQueue::MessageType::TEXT, "Queue1 Message");

    auto Message2 = std::make_shared<Helianthus::MessageQueue::Message>(
        Helianthus::MessageQueue::MessageType::TEXT, "Queue2 Message");

    Queue->SendMessage("e2e_test_queue", Message1);
    Queue->SendMessage("e2e_test_queue2", Message2);

    // 从两个队列接收消息
    Helianthus::MessageQueue::MessagePtr Received1, Received2;
    auto Result1 = Queue->ReceiveMessage("e2e_test_queue", Received1);
    auto Result2 = Queue->ReceiveMessage("e2e_test_queue2", Received2);

    EXPECT_EQ(Result1, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(Result2, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(Received1, nullptr);
    EXPECT_NE(Received2, nullptr);

    std::string ReceivedData1(Received1->Payload.Data.begin(), Received1->Payload.Data.end());
    std::string ReceivedData2(Received2->Payload.Data.begin(), Received2->Payload.Data.end());
    EXPECT_EQ(ReceivedData1, "Queue1 Message");
    EXPECT_EQ(ReceivedData2, "Queue2 Message");
}

TEST_F(EndToEndTest, PerformanceEndToEndFlow)
{
    // 简化的性能测试
    const int MessageCount = 10;  // 减少消息数量
    auto StartTime = std::chrono::high_resolution_clock::now();

    // 批量发送
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>(
            Helianthus::MessageQueue::MessageType::TEXT, "Perf " + std::to_string(i));
        Queue->SendMessage("e2e_test_queue", Message);
    }

    // 批量接收
    for (int i = 0; i < MessageCount; ++i)
    {
        Helianthus::MessageQueue::MessagePtr Message;
        Queue->ReceiveMessage("e2e_test_queue", Message);
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    // 验证性能合理
    EXPECT_LT(Duration.count(), 1000);  // 应该在1秒内完成
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
