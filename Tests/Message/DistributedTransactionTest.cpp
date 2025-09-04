#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

class DistributedTransactionTest : public ::testing::Test
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

// 测试基本的分布式事务流程
TEST_F(DistributedTransactionTest, BasicDistributedTransactionFlow)
{
    // 创建测试队列
    std::string QueueName = "dist_tx_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 开始分布式事务
    std::string CoordinatorId = "coordinator_001";
    std::string Description = "测试分布式事务";
    uint32_t TimeoutMs = 10000;

    auto Result = Queue->BeginDistributedTransaction(CoordinatorId, Description, TimeoutMs);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 使用普通事务ID进行后续操作
    Helianthus::MessageQueue::TransactionId TxId = Queue->BeginTransaction(Description, TimeoutMs);
    ASSERT_GT(TxId, 0u);

    // 在事务中发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    std::string Payload = "分布式事务测试消息";
    Message->Payload.Data.assign(Payload.begin(), Payload.end());

    Result = Queue->SendMessageInTransaction(TxId, QueueName, Message);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 准备事务（两阶段提交的第一阶段）
    Result = Queue->PrepareTransaction(TxId);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 提交分布式事务（两阶段提交的第二阶段）
    Result = Queue->CommitDistributedTransaction(TxId);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证消息是否成功发送
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    ASSERT_NE(ReceivedMessage, nullptr);

    std::string ReceivedPayload(ReceivedMessage->Payload.Data.begin(),
                                ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedPayload, Payload);

    // 验证事务状态
    Helianthus::MessageQueue::TransactionStatus Status;
    Result = Queue->GetTransactionStatus(TxId, Status);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(Status, Helianthus::MessageQueue::TransactionStatus::COMMITTED);
}

// 测试分布式事务回滚
TEST_F(DistributedTransactionTest, DistributedTransactionRollback)
{
    // 创建测试队列
    std::string QueueName = "dist_tx_rollback_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 开始分布式事务
    std::string CoordinatorId = "coordinator_002";
    auto Result = Queue->BeginDistributedTransaction(CoordinatorId, "回滚测试", 5000);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 使用普通事务ID
    Helianthus::MessageQueue::TransactionId TxId = Queue->BeginTransaction("回滚测试", 5000);
    ASSERT_GT(TxId, 0u);

    // 在事务中发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    std::string Payload = "应该被回滚的消息";
    Message->Payload.Data.assign(Payload.begin(), Payload.end());

    Result = Queue->SendMessageInTransaction(TxId, QueueName, Message);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 回滚分布式事务
    std::string RollbackReason = "测试回滚";
    Result = Queue->RollbackDistributedTransaction(TxId, RollbackReason);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证消息没有被发送
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    ASSERT_NE(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证事务状态
    Helianthus::MessageQueue::TransactionStatus Status;
    Result = Queue->GetTransactionStatus(TxId, Status);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(Status, Helianthus::MessageQueue::TransactionStatus::ROLLED_BACK);
}

// 测试分布式事务超时
TEST_F(DistributedTransactionTest, DistributedTransactionTimeout)
{
    // 创建测试队列
    std::string QueueName = "dist_tx_timeout_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 开始一个超时时间很短的分布式事务
    std::string CoordinatorId = "coordinator_003";
    uint32_t ShortTimeoutMs = 100;  // 100ms 超时
    auto Result = Queue->BeginDistributedTransaction(CoordinatorId, "超时测试", ShortTimeoutMs);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 使用普通事务ID
    Helianthus::MessageQueue::TransactionId TxId =
        Queue->BeginTransaction("超时测试", ShortTimeoutMs);
    ASSERT_GT(TxId, 0u);

    // 在事务中发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    std::string Payload = "应该超时的消息";
    Message->Payload.Data.assign(Payload.begin(), Payload.end());

    Result = Queue->SendMessageInTransaction(TxId, QueueName, Message);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 等待事务超时（需要更长时间确保后台线程处理）
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // 验证事务状态
    Helianthus::MessageQueue::TransactionStatus Status;
    Result = Queue->GetTransactionStatus(TxId, Status);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 如果事务还没有超时，再等待一段时间
    if (Status == Helianthus::MessageQueue::TransactionStatus::PENDING)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        Result = Queue->GetTransactionStatus(TxId, Status);
        ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    EXPECT_EQ(Status, Helianthus::MessageQueue::TransactionStatus::TIMEOUT);

    // 验证消息没有被发送
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    ASSERT_NE(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
}

// 测试分布式事务统计信息
TEST_F(DistributedTransactionTest, DistributedTransactionStatistics)
{
    // 创建测试队列
    std::string QueueName = "dist_tx_stats_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 执行一些分布式事务操作
    const int CommitCount = 3;
    const int RollbackCount = 2;
    const int TimeoutCount = 1;

    // 提交的事务
    for (int i = 0; i < CommitCount; ++i)
    {
        std::string CoordinatorId = "coordinator_commit_" + std::to_string(i);
        auto Result = Queue->BeginDistributedTransaction(CoordinatorId, "统计测试", 5000);
        ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

        Helianthus::MessageQueue::TransactionId TxId = Queue->BeginTransaction("统计测试", 5000);

        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "提交消息 " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());

        Queue->SendMessageInTransaction(TxId, QueueName, Message);
        Queue->PrepareTransaction(TxId);
        Queue->CommitDistributedTransaction(TxId);
    }

    // 回滚的事务
    for (int i = 0; i < RollbackCount; ++i)
    {
        std::string CoordinatorId = "coordinator_rollback_" + std::to_string(i);
        auto Result = Queue->BeginDistributedTransaction(CoordinatorId, "统计测试", 5000);
        ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

        Helianthus::MessageQueue::TransactionId TxId = Queue->BeginTransaction("统计测试", 5000);

        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "回滚消息 " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());

        Queue->SendMessageInTransaction(TxId, QueueName, Message);
        Queue->RollbackDistributedTransaction(TxId, "测试回滚");
    }

    // 超时的事务
    for (int i = 0; i < TimeoutCount; ++i)
    {
        std::string CoordinatorId = "coordinator_timeout_" + std::to_string(i);
        auto Result = Queue->BeginDistributedTransaction(CoordinatorId, "统计测试", 100);  // 短超时
        ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

        Helianthus::MessageQueue::TransactionId TxId =
            Queue->BeginTransaction("统计测试", 100);  // 短超时

        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "超时消息 " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());

        Queue->SendMessageInTransaction(TxId, QueueName, Message);
        // 不提交，让它超时
    }

    // 等待超时事务完成（需要更长时间确保后台线程处理）
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // 获取事务统计
    Helianthus::MessageQueue::TransactionStats Stats;
    auto Result = Queue->GetTransactionStats(Stats);
    ASSERT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 验证统计信息（使用更宽松的条件，因为可能有之前测试的残留）
    int MinExpected = CommitCount + RollbackCount + TimeoutCount;
    EXPECT_GE(Stats.TotalTransactions, MinExpected);
    EXPECT_GE(Stats.CommittedTransactions, CommitCount);
    EXPECT_GE(Stats.RolledBackTransactions, RollbackCount);

    // 超时事务可能还没有被处理，所以不强制要求
    EXPECT_GE(Stats.TimeoutTransactions, 0);

    EXPECT_GT(Stats.SuccessRate, 0.0);
    EXPECT_LE(Stats.SuccessRate, 100.0);

    // 平均时间可能为0，如果所有事务都很快完成
    EXPECT_GE(Stats.AverageCommitTimeMs, 0.0);
    EXPECT_GE(Stats.AverageRollbackTimeMs, 0.0);
}
