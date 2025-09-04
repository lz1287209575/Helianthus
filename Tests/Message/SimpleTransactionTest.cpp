#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <memory>

#include <gtest/gtest.h>

using namespace Helianthus::MessageQueue;

class SimpleTransactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化消息队列
        QueueResult InitResult = MQ.Initialize();
        ASSERT_EQ(InitResult, QueueResult::SUCCESS) << "消息队列初始化应该成功";

        // 创建测试队列
        QueueConfig Config;
        Config.Name = "simple_transaction_test_queue";
        Config.MaxSize = 1000;
        Config.EnableBatching = false;

        QueueResult Result = MQ.CreateQueue(Config);
        ASSERT_EQ(Result, QueueResult::SUCCESS) << "应该能创建队列";
    }

    void TearDown() override
    {
        // 清理测试队列
        MQ.DeleteQueue("simple_transaction_test_queue");
    }

    MessageQueue MQ;
};

// 测试基本事务创建
TEST_F(SimpleTransactionTest, BasicTransactionCreation)
{
    // 创建事务
    TransactionId TxId = MQ.BeginTransaction("simple_test_transaction", 30000);

    EXPECT_NE(TxId, 0) << "事务ID应该不为0";

    // 检查事务状态
    TransactionStatus Status;
    QueueResult Result = MQ.GetTransactionStatus(TxId, Status);

    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能获取事务状态";
    EXPECT_EQ(Status, TransactionStatus::PENDING) << "新事务应该是PENDING状态";
}

// 测试事务内发送消息
TEST_F(SimpleTransactionTest, SendMessageInTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("simple_test_transaction", 30000);
    ASSERT_NE(TxId, 0);

    // 创建测试消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");

    // 在事务中发送消息
    QueueResult Result = MQ.SendMessageInTransaction(TxId, "simple_transaction_test_queue", Msg);

    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能在事务中发送消息";
}

// 测试事务提交
TEST_F(SimpleTransactionTest, CommitTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("simple_test_transaction", 30000);
    ASSERT_NE(TxId, 0);

    // 在事务中发送消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");

    QueueResult Result = MQ.SendMessageInTransaction(TxId, "simple_transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);

    // 提交事务
    Result = MQ.CommitTransaction(TxId);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务提交应该成功";
}

// 测试事务回滚
TEST_F(SimpleTransactionTest, RollbackTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("simple_test_transaction", 30000);
    ASSERT_NE(TxId, 0);

    // 在事务中发送消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");

    QueueResult Result = MQ.SendMessageInTransaction(TxId, "simple_transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);

    // 回滚事务
    Result = MQ.RollbackTransaction(TxId, "test rollback");
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务回滚应该成功";
}

// 测试事务统计
TEST_F(SimpleTransactionTest, TransactionStatistics)
{
    // 创建并提交一个事务
    TransactionId TxId = MQ.BeginTransaction("simple_test_transaction", 30000);
    ASSERT_NE(TxId, 0);

    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");

    QueueResult Result = MQ.SendMessageInTransaction(TxId, "simple_transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);

    Result = MQ.CommitTransaction(TxId);
    ASSERT_EQ(Result, QueueResult::SUCCESS);

    // 获取事务统计
    TransactionStats Stats;
    Result = MQ.GetTransactionStats(Stats);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能获取事务统计";

    // 验证统计信息
    EXPECT_GE(Stats.TotalTransactions, 1) << "总事务数应该至少为1";
    EXPECT_GE(Stats.CommittedTransactions, 1) << "提交的事务数应该至少为1";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
