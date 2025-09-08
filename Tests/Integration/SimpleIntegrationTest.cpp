#include "Shared/MessageQueue/MessageQueue.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

class SimpleIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
        Queue->Initialize();
        
        // 创建测试队列
        Helianthus::MessageQueue::QueueConfig Config;
        Config.Name = "SimpleIntegrationTest";
        Config.MaxSize = 100;
        Queue->CreateQueue(Config);
    }

    void TearDown() override
    {
        // 清理队列
        if (Queue && Queue->IsInitialized()) {
            Queue->PurgeQueue("SimpleIntegrationTest");
            Queue->Shutdown();
        }
    }

    std::unique_ptr<Helianthus::MessageQueue::MessageQueue> Queue;
};

TEST_F(SimpleIntegrationTest, BasicMessageFlow)
{
    // 基础消息流测试
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    std::string MessageText = "Hello Integration Test";
    Message->Payload.Data = std::vector<char>(MessageText.begin(), MessageText.end());

    // 发送消息
    auto SendResult = Queue->SendMessage("SimpleIntegrationTest", Message);
    EXPECT_EQ(SendResult, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    auto ReceiveResult = Queue->ReceiveMessage("SimpleIntegrationTest", ReceivedMessage);
    EXPECT_EQ(ReceiveResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    EXPECT_EQ(std::string(ReceivedMessage->Payload.Data.begin(), ReceivedMessage->Payload.Data.end()), "Hello Integration Test");
}

TEST_F(SimpleIntegrationTest, QueueManagement)
{
    // 队列管理测试
    EXPECT_TRUE(Queue->QueueExists("SimpleIntegrationTest"));
    
    Helianthus::MessageQueue::QueueConfig Config;
    auto GetResult = Queue->GetQueueInfo("SimpleIntegrationTest", Config);
    EXPECT_EQ(GetResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_EQ(Config.Name, "SimpleIntegrationTest");
}

TEST_F(SimpleIntegrationTest, MultipleMessages)
{
    // 多消息测试
    const int MessageCount = 3;
    
    // 发送多个消息
    for (int i = 0; i < MessageCount; ++i) {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string MessageText = "Message " + std::to_string(i);
        Message->Payload.Data = std::vector<char>(MessageText.begin(), MessageText.end());
        
        auto SendResult = Queue->SendMessage("SimpleIntegrationTest", Message);
        EXPECT_EQ(SendResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }
    
    // 接收所有消息
    for (int i = 0; i < MessageCount; ++i) {
        Helianthus::MessageQueue::MessagePtr ReceivedMessage;
        auto ReceiveResult = Queue->ReceiveMessage("SimpleIntegrationTest", ReceivedMessage);
        EXPECT_EQ(ReceiveResult, Helianthus::MessageQueue::QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
        std::string ReceivedText(ReceivedMessage->Payload.Data.begin(), ReceivedMessage->Payload.Data.end());
        EXPECT_EQ(ReceivedText, "Message " + std::to_string(i));
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}