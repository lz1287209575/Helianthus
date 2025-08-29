#include <gtest/gtest.h>
#include <string>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

using namespace Helianthus::MessageQueue;

class TransactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Queue = std::make_unique<MessageQueue>();
        ASSERT_EQ(Queue->Initialize(), QueueResult::SUCCESS);
        QueueConfig C; C.Name = QueueName; C.Persistence = PersistenceMode::MEMORY_ONLY;
        ASSERT_EQ(Queue->CreateQueue(C), QueueResult::SUCCESS);
    }

    void TearDown() override
    {
        Queue->Shutdown();
    }

    std::unique_ptr<MessageQueue> Queue;
    std::string QueueName{"tx_demo"};
};

TEST_F(TransactionTest, CommitTransactionEnqueuesMessage)
{
    // Begin a transaction
    auto TxId = Queue->BeginTransaction("commit_flow", 5000);
    ASSERT_GT(TxId, 0u);

    // Stage a message within transaction
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    const std::string Payload = "HelloTx";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(Queue->SendMessageInTransaction(TxId, QueueName, Msg), QueueResult::SUCCESS);

    // Commit
    ASSERT_EQ(Queue->CommitTransaction(TxId), QueueResult::SUCCESS);

    // Verify message is available
    MessagePtr Out;
    auto R = Queue->ReceiveMessage(QueueName, Out, 1000);
    ASSERT_EQ(R, QueueResult::SUCCESS);
    ASSERT_NE(Out, nullptr);
    std::string Received(Out->Payload.Data.begin(), Out->Payload.Data.end());
    EXPECT_EQ(Received, Payload);

    // Stats reflect a committed transaction
    TransactionStats TS{};
    ASSERT_EQ(Queue->GetTransactionStats(TS), QueueResult::SUCCESS);
    EXPECT_GE(TS.TotalTransactions, 1u);
    EXPECT_GE(TS.CommittedTransactions, 1u);
}

TEST_F(TransactionTest, RollbackTransactionDoesNotEnqueue)
{
    // Begin a transaction
    auto TxId = Queue->BeginTransaction("rollback_flow", 5000);
    ASSERT_GT(TxId, 0u);

    // Stage a message within transaction
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    const std::string Payload = "HelloRollback";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(Queue->SendMessageInTransaction(TxId, QueueName, Msg), QueueResult::SUCCESS);

    // Rollback
    ASSERT_EQ(Queue->RollbackTransaction(TxId, "test"), QueueResult::SUCCESS);

    // Verify message is NOT available
    MessagePtr Out;
    auto R = Queue->ReceiveMessage(QueueName, Out, 100); // small timeout
    // Either timeout or queue empty success without message
    EXPECT_TRUE(R == QueueResult::TIMEOUT || (R == QueueResult::SUCCESS && Out == nullptr));

    // Stats reflect a rolled-back transaction
    TransactionStats TS{};
    ASSERT_EQ(Queue->GetTransactionStats(TS), QueueResult::SUCCESS);
    EXPECT_GE(TS.TotalTransactions, 1u);
    EXPECT_GE(TS.RolledBackTransactions, 1u);
}

TEST_F(TransactionTest, AcknowledgeMessageInTransaction)
{
    // Send and receive a message to create a pending-ack state
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    const std::string Payload = "AckMe";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(Queue->SendMessage(QueueName, Msg), QueueResult::SUCCESS);

    MessagePtr Out;
    ASSERT_EQ(Queue->ReceiveMessage(QueueName, Out, 1000), QueueResult::SUCCESS);
    ASSERT_NE(Out, nullptr);
    auto Id = Out->Header.Id;

    // Acknowledge within a transaction
    auto TxId = Queue->BeginTransaction("ack_flow", 5000);
    ASSERT_EQ(Queue->AcknowledgeMessageInTransaction(TxId, QueueName, Id), QueueResult::SUCCESS);
    ASSERT_EQ(Queue->CommitTransaction(TxId), QueueResult::SUCCESS);

    // The message should not be available anymore
    MessagePtr Out2;
    auto R = Queue->ReceiveMessage(QueueName, Out2, 100);
    EXPECT_TRUE(R == QueueResult::TIMEOUT || (R == QueueResult::SUCCESS && Out2 == nullptr));
}

TEST_F(TransactionTest, RejectMessageInTransactionRequeues)
{
    // Send and receive a message to create a pending-ack state
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    const std::string Payload = "RejectMe";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(Queue->SendMessage(QueueName, Msg), QueueResult::SUCCESS);

    MessagePtr Out;
    ASSERT_EQ(Queue->ReceiveMessage(QueueName, Out, 1000), QueueResult::SUCCESS);
    ASSERT_NE(Out, nullptr);
    auto Id = Out->Header.Id;

    // Reject (with requeue default=true) within a transaction
    auto TxId = Queue->BeginTransaction("reject_flow", 5000);
    ASSERT_EQ(Queue->RejectMessageInTransaction(TxId, QueueName, Id, "test"), QueueResult::SUCCESS);
    ASSERT_EQ(Queue->CommitTransaction(TxId), QueueResult::SUCCESS);

    // The message should be requeued and available again
    MessagePtr Out2;
    auto R = Queue->ReceiveMessage(QueueName, Out2, 1000);
    ASSERT_EQ(R, QueueResult::SUCCESS);
    ASSERT_NE(Out2, nullptr);
    std::string Received(Out2->Payload.Data.begin(), Out2->Payload.Data.end());
    EXPECT_EQ(Received, Payload);
}


