#include <gtest/gtest.h>
#include "Message/MessageQueue.h"
#include "Message/MessageTypes.h"
#include <thread>
#include <chrono>
#include <set>

using namespace Helianthus::Message;

class MessageQueueTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Config_.MaxQueueSize = 100;
        Config_.MaxMessageSize = 1024 * 1024; // 1MB
        Config_.DefaultTimeoutMs = 5000;
        Config_.MaxRetries = 3;
        Config_.EnablePersistence = false;
        Config_.EnableCompression = false;
        Config_.EnableEncryption = false;
    }

    MessageQueueConfig Config_;
};

TEST_F(MessageQueueTest, ConstructorInitializesCorrectly)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    EXPECT_TRUE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetSize(), 0);
    EXPECT_EQ(Queue.GetSize(), 0);
    EXPECT_FALSE(Queue.IsFull());
}

TEST_F(MessageQueueTest, EnqueueAndDequeueWorksCorrectly)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    auto Msg = Message::Create(MessageType::GAME_PLAYER_JOIN);
    Msg->SetPayload("Test player join");
    
    // Enqueue message
    auto Result = Queue.Enqueue(std::move(Msg));
    EXPECT_EQ(Result, MessageResult::SUCCESS);
    
    EXPECT_FALSE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetSize(), 1);
    
    // Dequeue message
    auto DequeuedMsg = Queue.Dequeue();
    EXPECT_NE(DequeuedMsg, nullptr);
    EXPECT_EQ(DequeuedMsg->GetMessageType(), MessageType::GAME_PLAYER_JOIN);
    EXPECT_EQ(DequeuedMsg->GetJsonPayload(), "Test player join");
    
    EXPECT_TRUE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetSize(), 0);
}

TEST_F(MessageQueueTest, PriorityOrderingWorksCorrectly)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Create messages with different priorities
    auto LowPriorityMsg = Message::Create(MessageType::GAME_STATE_UPDATE);
    LowPriorityMsg->SetPriority(MessagePriority::LOW);
    LowPriorityMsg->SetPayload("Low priority");
    
    auto HighPriorityMsg = Message::Create(MessageType::SYSTEM_SHUTDOWN);
    HighPriorityMsg->SetPriority(MessagePriority::CRITICAL);
    HighPriorityMsg->SetPayload("Critical priority");
    
    auto MediumPriorityMsg = Message::Create(MessageType::AUTH_LOGIN_REQUEST);
    MediumPriorityMsg->SetPriority(MessagePriority::HIGH);
    MediumPriorityMsg->SetPayload("High priority");
    
    // Enqueue in wrong order
    Queue.Enqueue(std::move(LowPriorityMsg));
    Queue.Enqueue(std::move(HighPriorityMsg));
    Queue.Enqueue(std::move(MediumPriorityMsg));
    
    EXPECT_EQ(Queue.GetSize(), 3);
    
    // Dequeue should return in priority order (Critical, High, Low)
    auto Msg1 = Queue.Dequeue();
    EXPECT_EQ(Msg1->GetPriority(), MessagePriority::CRITICAL);
    EXPECT_EQ(Msg1->GetJsonPayload(), "Critical priority");
    
    auto Msg2 = Queue.Dequeue();
    EXPECT_EQ(Msg2->GetPriority(), MessagePriority::HIGH);
    EXPECT_EQ(Msg2->GetJsonPayload(), "High priority");
    
    auto Msg3 = Queue.Dequeue();
    EXPECT_EQ(Msg3->GetPriority(), MessagePriority::LOW);
    EXPECT_EQ(Msg3->GetJsonPayload(), "Low priority");
}

TEST_F(MessageQueueTest, MaxMessagesLimitWorks)
{
    Config_.MaxQueueSize = 2;
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Fill the queue to maximum
    auto Msg1 = Message::Create(MessageType::GAME_PLAYER_JOIN);
    auto Msg2 = Message::Create(MessageType::GAME_PLAYER_LEAVE);
    
    EXPECT_EQ(Queue.Enqueue(std::move(Msg1)), MessageResult::SUCCESS);
    EXPECT_EQ(Queue.Enqueue(std::move(Msg2)), MessageResult::SUCCESS);
    
    EXPECT_TRUE(Queue.IsFull());
    EXPECT_EQ(Queue.GetSize(), 2);
    
    // Try to add one more message - should fail
    auto Msg3 = Message::Create(MessageType::NETWORK_DATA_RECEIVED);
    EXPECT_EQ(Queue.Enqueue(std::move(Msg3)), MessageResult::QUEUE_FULL);
}

TEST_F(MessageQueueTest, DequeueAllMessagesWorksCorrectly)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Add multiple messages
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = Message::Create(MessageType::GAME_STATE_UPDATE);
        Msg->SetPayload("Message " + std::to_string(i));
        Queue.Enqueue(std::move(Msg));
    }
    
    EXPECT_EQ(Queue.GetSize(), 5);
    
    // Dequeue all messages
    auto AllMessages = Queue.DequeueBatch(5);
    EXPECT_EQ(AllMessages.size(), 5);
    EXPECT_TRUE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetSize(), 0);
    
    // Verify all messages are returned (order may vary due to priority queue)
    std::set<std::string> ExpectedPayloads;
    std::set<std::string> ActualPayloads;
    
    for (int i = 0; i < 5; ++i)
    {
        ExpectedPayloads.insert("Message " + std::to_string(i));
    }
    
    for (const auto& Msg : AllMessages)
    {
        ActualPayloads.insert(Msg->GetJsonPayload());
    }
    
    EXPECT_EQ(ActualPayloads, ExpectedPayloads);
}

TEST_F(MessageQueueTest, PeekWorksCorrectly)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    auto Msg = Message::Create(MessageType::AUTH_LOGIN_RESPONSE);
    Msg->SetPayload("Peek test message");
    Queue.Enqueue(std::move(Msg));
    
    // Peek should return the message without removing it
    auto PeekedMsg = Queue.Peek();
    EXPECT_NE(PeekedMsg, nullptr);
    EXPECT_EQ(PeekedMsg->GetJsonPayload(), "Peek test message");
    
    // Queue should still have the message
    EXPECT_FALSE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetSize(), 1);
    
    // Dequeue should return the same message
    auto DequeuedMsg = Queue.Dequeue();
    EXPECT_NE(DequeuedMsg, nullptr);
    EXPECT_EQ(DequeuedMsg->GetJsonPayload(), "Peek test message");
    EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(MessageQueueTest, FilteredOperationsWork)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Add messages of different types
    auto GameMsg = Message::Create(MessageType::GAME_PLAYER_JOIN);
    GameMsg->SetPayload("Game message");
    
    auto AuthMsg = Message::Create(MessageType::AUTH_LOGIN_REQUEST);
    AuthMsg->SetPayload("Auth message");
    
    auto NetworkMsg = Message::Create(MessageType::NETWORK_DATA_RECEIVED);
    NetworkMsg->SetPayload("Network message");
    
    Queue.Enqueue(std::move(GameMsg));
    Queue.Enqueue(std::move(AuthMsg));
    Queue.Enqueue(std::move(NetworkMsg));
    
    // Filter by message type
    auto GameMessages = Queue.DequeueByType(MessageType::GAME_PLAYER_JOIN, 1);
    EXPECT_EQ(GameMessages.size(), 1);
    EXPECT_EQ(GameMessages[0]->GetJsonPayload(), "Game message");
    
    // Clear by type - remove auth messages
    Queue.ClearByType(MessageType::AUTH_LOGIN_REQUEST);
    EXPECT_EQ(Queue.GetSize(), 1); // Should have 1 remaining (NETWORK_DATA_RECEIVED)
}

TEST_F(MessageQueueTest, BatchOperationsWork)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Create batch of messages
    std::vector<MessagePtr> Messages;
    for (int i = 0; i < 3; ++i)
    {
        auto Msg = Message::Create(MessageType::GAME_STATE_UPDATE);
        Msg->SetPayload("Batch message " + std::to_string(i));
        Messages.push_back(Msg);
    }
    
    // Enqueue batch
    for (auto& Msg : Messages)
    {
        auto Result = Queue.Enqueue(Msg);
        EXPECT_EQ(Result, MessageResult::SUCCESS);
    }
    
    EXPECT_EQ(Queue.GetSize(), 3);
    
    // Dequeue batch
    auto DequeuedMessages = Queue.DequeueBatch(2);
    EXPECT_EQ(DequeuedMessages.size(), 2);
    EXPECT_EQ(Queue.GetSize(), 1);
}

TEST_F(MessageQueueTest, ThreadSafetyBasicTest)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    std::atomic<int> EnqueueCount = 0;
    std::atomic<int> DequeueCount = 0;
    
    // Producer thread
    std::thread Producer([&Queue, &EnqueueCount]() {
        for (int i = 0; i < 10; ++i)
        {
            auto Msg = Message::Create(MessageType::GAME_STATE_UPDATE);
            Msg->SetPayload("Thread message " + std::to_string(i));
            if (Queue.Enqueue(std::move(Msg)) == MessageResult::SUCCESS)
            {
                EnqueueCount++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Consumer thread
    std::thread Consumer([&Queue, &DequeueCount]() {
        for (int i = 0; i < 10; ++i)
        {
            while (Queue.IsEmpty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            auto Msg = Queue.Dequeue();
            if (Msg)
            {
                DequeueCount++;
            }
        }
    });
    
    Producer.join();
    Consumer.join();
    
    EXPECT_EQ(EnqueueCount.load(), 10);
    EXPECT_EQ(DequeueCount.load(), 10);
    EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(MessageQueueTest, StatisticsWork)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Add some messages and track statistics
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = Message::Create(MessageType::GAME_PLAYER_JOIN);
        Msg->SetPayload("Stats test " + std::to_string(i));
        Queue.Enqueue(std::move(Msg));
    }
    
    auto Stats = Queue.GetStats();
    EXPECT_GT(Stats.QueueSize, 0);
    
    // Dequeue some messages
    Queue.Dequeue();
    Queue.Dequeue();
    
    Stats = Queue.GetStats();
    EXPECT_GT(Stats.QueueSize, 0);
}

TEST_F(MessageQueueTest, ClearOperationsWork)
{
    MessageQueue Queue;
    Queue.Initialize(Config_);
    
    // Add messages
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = Message::Create(MessageType::GAME_PLAYER_JOIN);
        Queue.Enqueue(std::move(Msg));
    }
    
    EXPECT_EQ(Queue.GetSize(), 5);
    
    // Clear all messages
    Queue.Clear();
    EXPECT_TRUE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetSize(), 0);
}