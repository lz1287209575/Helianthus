#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

using namespace Helianthus::MessageQueue;

class TransactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化消息队列
        QueueResult InitResult = MQ.Initialize();
        ASSERT_EQ(InitResult, QueueResult::SUCCESS) << "消息队列初始化应该成功";
        
        // 创建测试队列
        QueueConfig Config;
        Config.Name = "transaction_test_queue";
        Config.MaxSize = 10000;
        Config.EnableBatching = true;
        Config.BatchSize = 50;
        
        QueueResult Result = MQ.CreateQueue(Config);
        ASSERT_EQ(Result, QueueResult::SUCCESS) << "应该能创建队列";
    }

    void TearDown() override
    {
        // 清理测试队列
        MQ.DeleteQueue("transaction_test_queue");
    }

    MessageQueue MQ;
};

// 测试基本事务创建
TEST_F(TransactionTest, BasicTransactionCreation)
{
    // 创建事务
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    
    EXPECT_NE(TxId, 0) << "事务ID应该不为0";
    
    // 检查事务状态
    TransactionStatus Status;
    QueueResult Result = MQ.GetTransactionStatus(TxId, Status);
    
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能获取事务状态";
    EXPECT_EQ(Status, TransactionStatus::PENDING) << "新事务应该是PENDING状态";
}

// 测试事务内发送消息
TEST_F(TransactionTest, SendMessageInTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    ASSERT_NE(TxId, 0);
    
    // 创建测试消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");
    
    // 在事务中发送消息
    QueueResult Result = MQ.SendMessageInTransaction(TxId, "transaction_test_queue", Msg);
    
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能在事务中发送消息";
    
    // 验证消息在事务提交前不可见
    MessagePtr ReceivedMsg;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 100);
    
    EXPECT_NE(Result, QueueResult::SUCCESS) << "事务未提交时消息应该不可见";
}

// 测试事务提交
TEST_F(TransactionTest, CommitTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    ASSERT_NE(TxId, 0);
    
    // 在事务中发送消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");
    
    QueueResult Result = MQ.SendMessageInTransaction(TxId, "transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);
    
    // 提交事务
    Result = MQ.CommitTransaction(TxId);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务提交应该成功";
    
    // 验证消息在事务提交后可见
    MessagePtr ReceivedMsg;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 1000);
    
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务提交后消息应该可见";
    EXPECT_EQ(ReceivedMsg->Header.Id, 1) << "应该能接收到正确的消息";
}

// 测试事务回滚
TEST_F(TransactionTest, RollbackTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    ASSERT_NE(TxId, 0);
    
    // 在事务中发送消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");
    
    QueueResult Result = MQ.SendMessageInTransaction(TxId, "transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);
    
    // 回滚事务
    Result = MQ.RollbackTransaction(TxId, "test rollback");
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务回滚应该成功";
    
    // 验证消息在事务回滚后不可见
    MessagePtr ReceivedMsg;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 100);
    
    EXPECT_NE(Result, QueueResult::SUCCESS) << "事务回滚后消息应该不可见";
}

// 测试事务超时
TEST_F(TransactionTest, TransactionTimeout)
{
    // 创建短超时的事务
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 100); // 100ms超时
    ASSERT_NE(TxId, 0);
    
    // 等待超时
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 等待1秒，确保超时
    
    // 检查事务状态
    TransactionStatus Status;
    QueueResult StatusResult = MQ.GetTransactionStatus(TxId, Status);
    EXPECT_EQ(StatusResult, QueueResult::SUCCESS) << "应该能获取事务状态";
    
    // 如果事务还没有超时，再等待一段时间
    if (Status == TransactionStatus::PENDING)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 再等待1秒
        StatusResult = MQ.GetTransactionStatus(TxId, Status);
        EXPECT_EQ(StatusResult, QueueResult::SUCCESS) << "应该能获取事务状态";
    }
    
    EXPECT_EQ(Status, TransactionStatus::TIMEOUT) << "事务应该已超时";
    
    // 尝试提交已超时的事务
    QueueResult Result = MQ.CommitTransaction(TxId);
    EXPECT_NE(Result, QueueResult::SUCCESS) << "超时的事务应该无法提交";
}

// 测试事务内批量操作
TEST_F(TransactionTest, BatchOperationsInTransaction)
{
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    ASSERT_NE(TxId, 0);
    
    // 在事务中发送多个消息
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = std::make_shared<Message>();
        Msg->Header.Id = i + 1;
        Msg->Header.Type = MessageType::TEXT;
        Msg->Payload = MessagePayload("test message " + std::to_string(i));
        
        QueueResult Result = MQ.SendMessageInTransaction(TxId, "transaction_test_queue", Msg);
        EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能在事务中发送消息 " << i;
    }
    
    // 提交事务
    QueueResult Result = MQ.CommitTransaction(TxId);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务提交应该成功";
    
    // 验证所有消息都可见
    for (int i = 0; i < 5; ++i)
    {
        MessagePtr ReceivedMsg;
        Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 1000);
        
        EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能接收到消息 " << i;
        EXPECT_EQ(ReceivedMsg->Header.Id, i + 1) << "消息ID应该正确";
    }
}

// 测试事务内确认消息
TEST_F(TransactionTest, AcknowledgeMessageInTransaction)
{
    // 先发送一个消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");
    
    QueueResult Result = MQ.SendMessage("transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);
    
    // 接收消息
    MessagePtr ReceivedMsg;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 1000);
    ASSERT_EQ(Result, QueueResult::SUCCESS);
    
    // 创建事务并在事务中确认消息
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    ASSERT_NE(TxId, 0);
    
    Result = MQ.AcknowledgeMessageInTransaction(TxId, "transaction_test_queue", ReceivedMsg->Header.Id);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能在事务中确认消息";
    
    // 提交事务
    Result = MQ.CommitTransaction(TxId);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务提交应该成功";
    
    // 验证消息已被确认（无法再次接收）
    MessagePtr ReceivedMsg2;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg2, 100);
    EXPECT_NE(Result, QueueResult::SUCCESS) << "已确认的消息应该无法再次接收";
}

// 测试事务内拒收消息
TEST_F(TransactionTest, RejectMessageInTransaction)
{
    // 先发送一个消息
    auto Msg = std::make_shared<Message>();
    Msg->Header.Id = 1;
    Msg->Header.Type = MessageType::TEXT;
    Msg->Payload = MessagePayload("test message");
    
    QueueResult Result = MQ.SendMessage("transaction_test_queue", Msg);
    ASSERT_EQ(Result, QueueResult::SUCCESS);
    
    // 接收消息
    MessagePtr ReceivedMsg;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 1000);
    ASSERT_EQ(Result, QueueResult::SUCCESS);
    
    // 创建事务并在事务中拒收消息
    TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
    ASSERT_NE(TxId, 0);
    
    Result = MQ.RejectMessageInTransaction(TxId, "transaction_test_queue", ReceivedMsg->Header.Id, "test reject");
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能在事务中拒收消息";
    
    // 提交事务
    Result = MQ.CommitTransaction(TxId);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "事务提交应该成功";
    
    // 验证消息已被拒收（无法再次接收）
    MessagePtr ReceivedMsg2;
    Result = MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg2, 100);
    EXPECT_NE(Result, QueueResult::SUCCESS) << "已拒收的消息应该无法再次接收";
}

// 测试事务统计
TEST_F(TransactionTest, TransactionStatistics)
{
    // 创建多个事务并提交/回滚
    for (int i = 0; i < 3; ++i)
    {
        TransactionId TxId = MQ.BeginTransaction("test_transaction", 30000);
        ASSERT_NE(TxId, 0);
        
        auto Msg = std::make_shared<Message>();
        Msg->Header.Id = i + 1;
        Msg->Header.Type = MessageType::TEXT;
        Msg->Payload = MessagePayload("test message " + std::to_string(i));
        
        QueueResult Result = MQ.SendMessageInTransaction(TxId, "transaction_test_queue", Msg);
        ASSERT_EQ(Result, QueueResult::SUCCESS);
        
        if (i % 2 == 0)
        {
            // 偶数事务提交
            Result = MQ.CommitTransaction(TxId);
            EXPECT_EQ(Result, QueueResult::SUCCESS);
        }
        else
        {
            // 奇数事务回滚
            Result = MQ.RollbackTransaction(TxId, "test rollback");
            EXPECT_EQ(Result, QueueResult::SUCCESS);
        }
    }
    
    // 获取事务统计
    TransactionStats Stats;
    QueueResult Result = MQ.GetTransactionStats(Stats);
    EXPECT_EQ(Result, QueueResult::SUCCESS) << "应该能获取事务统计";
    
    // 验证统计信息
    EXPECT_GE(Stats.TotalTransactions, 3) << "总事务数应该至少为3";
    EXPECT_GE(Stats.CommittedTransactions, 2) << "提交的事务数应该至少为2";
    EXPECT_GE(Stats.RolledBackTransactions, 1) << "回滚的事务数应该至少为1";
}

// 测试并发事务
TEST_F(TransactionTest, ConcurrentTransactions)
{
    const int ThreadCount = 4;
    const int TransactionsPerThread = 10;
    std::vector<std::thread> Threads;
    std::atomic<int> SuccessCount{0};
    
    // 创建多个线程，每个线程执行多个事务
    for (int ThreadId = 0; ThreadId < ThreadCount; ++ThreadId)
    {
        Threads.emplace_back([this, ThreadId, TransactionsPerThread, &SuccessCount]() {
            for (int i = 0; i < TransactionsPerThread; ++i)
            {
                TransactionId TxId = MQ.BeginTransaction("concurrent_transaction", 30000);
                if (TxId == 0) continue;
                
                // 在事务中发送消息
                auto Msg = std::make_shared<Message>();
                Msg->Header.Id = ThreadId * TransactionsPerThread + i + 1;
                Msg->Header.Type = MessageType::TEXT;
                Msg->Payload = MessagePayload("concurrent message");
                
                QueueResult Result = MQ.SendMessageInTransaction(TxId, "transaction_test_queue", Msg);
                if (Result == QueueResult::SUCCESS)
                {
                    Result = MQ.CommitTransaction(TxId);
                    if (Result == QueueResult::SUCCESS)
                    {
                        SuccessCount++;
                    }
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& Thread : Threads)
    {
        Thread.join();
    }
    
    // 验证并发事务执行成功
    EXPECT_GT(SuccessCount.load(), 0) << "应该有成功的事务";
    
    // 验证消息数量
    int MessageCount = 0;
    MessagePtr ReceivedMsg;
    while (MQ.ReceiveMessage("transaction_test_queue", ReceivedMsg, 100) == QueueResult::SUCCESS)
    {
        MessageCount++;
    }
    
    EXPECT_EQ(MessageCount, SuccessCount.load()) << "消息数量应该等于成功的事务数";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
