#include <gtest/gtest.h>
#include "Message/MessageQueue.h"
#include "Message/MessageTypes.h"
#include <thread>
#include <chrono>

using namespace Helianthus::Message;

class MessageQueueTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Config_.MaxMessages = 100;
        Config_.EnableAutoDequeue = false;
        Config_.AutoDequeueIntervalMs = 1000;
        Config_.MaxQueueSizeBytes = 1024 * 1024; // 1MB
    }

    MessageQueueConfig Config_;
};

TEST_F(MessageQueueTest, ConstructorInitializesCorrectly)
{
    MessageQueue Queue(Config_);
    
    EXPECT_FALSE(Queue.HasMessages());
    EXPECT_EQ(Queue.GetMessageCount(), 0);
    EXPECT_EQ(Queue.GetQueueSize(), 0);
    EXPECT_FALSE(Queue.IsFull());
    EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(MessageQueueTest, EnqueueAndDequeueWorksCorrectly)
{
    MessageQueue Queue(Config_);
    
    auto Msg = Message::Create(MESSAGE_TYPE::GAME_PLAYER_JOIN);
    Msg->SetPayload("Test player join");
    
    // Enqueue message
    auto Result = Queue.EnqueueMessage(std::move(Msg));
    EXPECT_EQ(Result, MESSAGE_RESULT::SUCCESS);
    
    EXPECT_TRUE(Queue.HasMessages());
    EXPECT_EQ(Queue.GetMessageCount(), 1);
    EXPECT_FALSE(Queue.IsEmpty());
    
    // Dequeue message
    auto DequeuedMsg = Queue.DequeueMessage();
    EXPECT_NE(DequeuedMsg, nullptr);
    EXPECT_EQ(DequeuedMsg->GetMessageType(), MESSAGE_TYPE::GAME_PLAYER_JOIN);
    EXPECT_EQ(DequeuedMsg->GetJsonPayload(), "Test player join");
    
    EXPECT_FALSE(Queue.HasMessages());
    EXPECT_EQ(Queue.GetMessageCount(), 0);
    EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(MessageQueueTest, PriorityOrderingWorksCorrectly)
{
    MessageQueue Queue(Config_);
    
    // Create messages with different priorities
    auto LowPriorityMsg = Message::Create(MESSAGE_TYPE::GAME_STATE_UPDATE);
    LowPriorityMsg->SetPriority(MESSAGE_PRIORITY::LOW);
    LowPriorityMsg->SetPayload("Low priority");
    
    auto HighPriorityMsg = Message::Create(MESSAGE_TYPE::SYSTEM_SHUTDOWN);
    HighPriorityMsg->SetPriority(MESSAGE_PRIORITY::CRITICAL);
    HighPriorityMsg->SetPayload("Critical priority");
    
    auto MediumPriorityMsg = Message::Create(MESSAGE_TYPE::AUTH_LOGIN_REQUEST);
    MediumPriorityMsg->SetPriority(MESSAGE_PRIORITY::HIGH);
    MediumPriorityMsg->SetPayload("High priority");
    
    // Enqueue in wrong order
    Queue.EnqueueMessage(std::move(LowPriorityMsg));
    Queue.EnqueueMessage(std::move(HighPriorityMsg));
    Queue.EnqueueMessage(std::move(MediumPriorityMsg));
    
    EXPECT_EQ(Queue.GetMessageCount(), 3);
    
    // Dequeue should return in priority order (Critical, High, Low)
    auto Msg1 = Queue.DequeueMessage();
    EXPECT_EQ(Msg1->GetPriority(), MESSAGE_PRIORITY::CRITICAL);
    EXPECT_EQ(Msg1->GetJsonPayload(), "Critical priority");
    
    auto Msg2 = Queue.DequeueMessage();
    EXPECT_EQ(Msg2->GetPriority(), MESSAGE_PRIORITY::HIGH);
    EXPECT_EQ(Msg2->GetJsonPayload(), "High priority");
    
    auto Msg3 = Queue.DequeueMessage();
    EXPECT_EQ(Msg3->GetPriority(), MESSAGE_PRIORITY::LOW);
    EXPECT_EQ(Msg3->GetJsonPayload(), "Low priority");
}

TEST_F(MessageQueueTest, MaxMessagesLimitWorks)
{
    Config_.MaxMessages = 2;
    MessageQueue Queue(Config_);
    
    // Fill the queue to maximum
    auto Msg1 = Message::Create(MESSAGE_TYPE::GAME_PLAYER_JOIN);
    auto Msg2 = Message::Create(MESSAGE_TYPE::GAME_PLAYER_LEAVE);
    
    EXPECT_EQ(Queue.EnqueueMessage(std::move(Msg1)), MESSAGE_RESULT::SUCCESS);
    EXPECT_EQ(Queue.EnqueueMessage(std::move(Msg2)), MESSAGE_RESULT::SUCCESS);
    
    EXPECT_TRUE(Queue.IsFull());
    EXPECT_EQ(Queue.GetMessageCount(), 2);
    
    // Try to add one more message - should fail
    auto Msg3 = Message::Create(MESSAGE_TYPE::NETWORK_DATA_RECEIVED);
    EXPECT_EQ(Queue.EnqueueMessage(std::move(Msg3)), MESSAGE_RESULT::QUEUE_FULL);
}

TEST_F(MessageQueueTest, DequeueAllMessagesWorksCorrectly)
{
    MessageQueue Queue(Config_);
    
    // Add multiple messages
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = Message::Create(MESSAGE_TYPE::GAME_STATE_UPDATE);
        Msg->SetPayload("Message " + std::to_string(i));
        Queue.EnqueueMessage(std::move(Msg));
    }
    
    EXPECT_EQ(Queue.GetMessageCount(), 5);
    
    // Dequeue all messages
    auto AllMessages = Queue.DequeueAllMessages();
    EXPECT_EQ(AllMessages.size(), 5);
    EXPECT_TRUE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetMessageCount(), 0);
    
    // Verify messages are in correct order
    for (size_t i = 0; i < AllMessages.size(); ++i)
    {
        EXPECT_EQ(AllMessages[i]->GetJsonPayload(), "Message " + std::to_string(i));
    }
}

TEST_F(MessageQueueTest, PeekWorksCorrectly)
{
    MessageQueue Queue(Config_);
    
    auto Msg = Message::Create(MESSAGE_TYPE::AUTH_LOGIN_RESPONSE);
    Msg->SetPayload("Peek test message");
    Queue.EnqueueMessage(std::move(Msg));
    
    // Peek should return the message without removing it
    auto PeekedMsg = Queue.PeekNextMessage();
    EXPECT_NE(PeekedMsg, nullptr);
    EXPECT_EQ(PeekedMsg->GetJsonPayload(), "Peek test message");
    
    // Queue should still have the message
    EXPECT_TRUE(Queue.HasMessages());
    EXPECT_EQ(Queue.GetMessageCount(), 1);
    
    // Dequeue should return the same message
    auto DequeuedMsg = Queue.DequeueMessage();
    EXPECT_NE(DequeuedMsg, nullptr);
    EXPECT_EQ(DequeuedMsg->GetJsonPayload(), "Peek test message");
    EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(MessageQueueTest, FilteredOperationsWork)
{
    MessageQueue Queue(Config_);
    
    // Add messages of different types
    auto GameMsg = Message::Create(MESSAGE_TYPE::GAME_PLAYER_JOIN);
    GameMsg->SetPayload("Game message");
    
    auto AuthMsg = Message::Create(MESSAGE_TYPE::AUTH_LOGIN_REQUEST);
    AuthMsg->SetPayload("Auth message");
    
    auto NetworkMsg = Message::Create(MESSAGE_TYPE::NETWORK_DATA_RECEIVED);
    NetworkMsg->SetPayload("Network message");
    
    Queue.EnqueueMessage(std::move(GameMsg));
    Queue.EnqueueMessage(std::move(AuthMsg));
    Queue.EnqueueMessage(std::move(NetworkMsg));
    
    // Filter by message type
    auto GameMessages = Queue.GetMessagesByType(MESSAGE_TYPE::GAME_PLAYER_JOIN);
    EXPECT_EQ(GameMessages.size(), 1);
    EXPECT_EQ(GameMessages[0]->GetJsonPayload(), "Game message");
    
    // Get messages by priority
    auto NormalMessages = Queue.GetMessagesByPriority(MESSAGE_PRIORITY::NORMAL);
    EXPECT_EQ(NormalMessages.size(), 3); // All have default NORMAL priority
    
    // Clear by type - remove auth messages
    auto ClearedCount = Queue.ClearMessagesByType(MESSAGE_TYPE::AUTH_LOGIN_REQUEST);
    EXPECT_EQ(ClearedCount, 1);
    EXPECT_EQ(Queue.GetMessageCount(), 2);
}

TEST_F(MessageQueueTest, BatchOperationsWork)
{
    MessageQueue Queue(Config_);
    
    // Create batch of messages
    std::vector<std::unique_ptr<Message>> Messages;
    for (int i = 0; i < 3; ++i)
    {
        auto Msg = Message::Create(MESSAGE_TYPE::GAME_STATE_UPDATE);
        Msg->SetPayload("Batch message " + std::to_string(i));
        Messages.push_back(std::move(Msg));
    }
    
    // Enqueue batch
    auto Results = Queue.EnqueueMessages(std::move(Messages));
    EXPECT_EQ(Results.size(), 3);
    for (auto Result : Results)
    {
        EXPECT_EQ(Result, MESSAGE_RESULT::SUCCESS);
    }
    
    EXPECT_EQ(Queue.GetMessageCount(), 3);
    
    // Dequeue batch
    auto DequeuedMessages = Queue.DequeueMessages(2);
    EXPECT_EQ(DequeuedMessages.size(), 2);
    EXPECT_EQ(Queue.GetMessageCount(), 1);
}

TEST_F(MessageQueueTest, ThreadSafetyBasicTest)
{
    MessageQueue Queue(Config_);
    std::atomic<int> EnqueueCount = 0;
    std::atomic<int> DequeueCount = 0;
    
    // Producer thread
    std::thread Producer([&Queue, &EnqueueCount]() {
        for (int i = 0; i < 10; ++i)
        {
            auto Msg = Message::Create(MESSAGE_TYPE::GAME_STATE_UPDATE);
            Msg->SetPayload("Thread message " + std::to_string(i));
            if (Queue.EnqueueMessage(std::move(Msg)) == MESSAGE_RESULT::SUCCESS)
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
            while (!Queue.HasMessages())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            auto Msg = Queue.DequeueMessage();
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
    MessageQueue Queue(Config_);
    
    // Add some messages and track statistics
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = Message::Create(MESSAGE_TYPE::GAME_PLAYER_JOIN);
        Msg->SetPayload("Stats test " + std::to_string(i));
        Queue.EnqueueMessage(std::move(Msg));
    }
    
    auto Stats = Queue.GetStatistics();
    EXPECT_EQ(Stats.TotalEnqueued, 5);
    EXPECT_EQ(Stats.CurrentCount, 5);
    EXPECT_GT(Stats.TotalSizeBytes, 0);
    
    // Dequeue some messages
    Queue.DequeueMessage();
    Queue.DequeueMessage();
    
    Stats = Queue.GetStatistics();
    EXPECT_EQ(Stats.TotalEnqueued, 5);
    EXPECT_EQ(Stats.TotalDequeued, 2);
    EXPECT_EQ(Stats.CurrentCount, 3);
}

TEST_F(MessageQueueTest, ClearOperationsWork)
{
    MessageQueue Queue(Config_);
    
    // Add messages
    for (int i = 0; i < 5; ++i)
    {
        auto Msg = Message::Create(MESSAGE_TYPE::GAME_PLAYER_JOIN);
        Queue.EnqueueMessage(std::move(Msg));
    }
    
    EXPECT_EQ(Queue.GetMessageCount(), 5);
    
    // Clear all messages
    Queue.ClearAllMessages();
    EXPECT_TRUE(Queue.IsEmpty());
    EXPECT_EQ(Queue.GetMessageCount(), 0);
}