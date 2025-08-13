#include <gtest/gtest.h>
#include "Message/Message.h"
#include "Message/MessageTypes.h"

using namespace Helianthus::Message;

class MessageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Cleanup code if needed  
    }
};

TEST_F(MessageTest, ConstructorInitializesCorrectly)
{
    Message Msg(MESSAGE_TYPE::GAME_PLAYER_JOIN);
    
    EXPECT_EQ(Msg.GetMessageType(), MESSAGE_TYPE::GAME_PLAYER_JOIN);
    EXPECT_NE(Msg.GetMessageId(), InvalidMessageId);
    EXPECT_GT(Msg.GetTimestamp(), 0);
    EXPECT_EQ(Msg.GetPayloadSize(), 0);
    EXPECT_FALSE(Msg.HasPayload());
}

TEST_F(MessageTest, SetAndGetPayloadWorksCorrectly)
{
    Message Msg(MESSAGE_TYPE::GAME_STATE_UPDATE);
    
    std::string TestPayload = "Hello, World!";
    Msg.SetPayload(TestPayload);
    
    EXPECT_TRUE(Msg.HasPayload());
    EXPECT_EQ(Msg.GetPayloadSize(), TestPayload.size());
    EXPECT_EQ(Msg.GetJsonPayload(), TestPayload);
}

TEST_F(MessageTest, SerializeAndDeserializeWorksCorrectly)
{
    Message OriginalMsg(MESSAGE_TYPE::NETWORK_DATA_RECEIVED);
    OriginalMsg.SetSenderId(123);
    OriginalMsg.SetReceiverId(456);
    OriginalMsg.SetPayload("Test message payload");
    OriginalMsg.SetPriority(MESSAGE_PRIORITY::HIGH);
    
    // Serialize
    auto SerializedData = OriginalMsg.Serialize();
    EXPECT_GT(SerializedData.size(), 0);
    
    // Deserialize
    Message DeserializedMsg;
    bool DeserializeResult = DeserializedMsg.Deserialize(SerializedData);
    
    EXPECT_TRUE(DeserializeResult);
    EXPECT_EQ(DeserializedMsg.GetMessageType(), OriginalMsg.GetMessageType());
    EXPECT_EQ(DeserializedMsg.GetSenderId(), OriginalMsg.GetSenderId());
    EXPECT_EQ(DeserializedMsg.GetReceiverId(), OriginalMsg.GetReceiverId());
    EXPECT_EQ(DeserializedMsg.GetJsonPayload(), OriginalMsg.GetJsonPayload());
    EXPECT_EQ(DeserializedMsg.GetPriority(), OriginalMsg.GetPriority());
}

TEST_F(MessageTest, ChecksumValidationWorks)
{
    Message Msg(MESSAGE_TYPE::SYSTEM_HEARTBEAT);
    Msg.SetPayload("Checksum test payload");
    
    // Update checksum
    Msg.UpdateChecksum();
    EXPECT_TRUE(Msg.ValidateChecksum());
    
    // Manually corrupt checksum
    auto& Header = Msg.GetHeader();
    Header.Checksum = 0xDEADBEEF;
    EXPECT_FALSE(Msg.ValidateChecksum());
}

TEST_F(MessageTest, MessageValidationWorks)
{
    Message ValidMsg(MESSAGE_TYPE::GAME_PLAYER_JOIN);
    ValidMsg.SetPayload("Valid message");
    EXPECT_TRUE(ValidMsg.IsValid());
    
    Message InvalidMsg;
    auto& InvalidHeader = InvalidMsg.GetHeader();
    InvalidHeader.MessageId = InvalidMessageId;
    EXPECT_FALSE(InvalidMsg.IsValid());
}

TEST_F(MessageTest, MessagePropertiesWorkCorrectly)
{
    Message Msg(MESSAGE_TYPE::AUTH_LOGIN_REQUEST);
    
    // Test all property setters and getters
    Msg.SetMessageType(MESSAGE_TYPE::AUTH_LOGIN_RESPONSE);
    EXPECT_EQ(Msg.GetMessageType(), MESSAGE_TYPE::AUTH_LOGIN_RESPONSE);
    
    Msg.SetPriority(MESSAGE_PRIORITY::CRITICAL);
    EXPECT_EQ(Msg.GetPriority(), MESSAGE_PRIORITY::CRITICAL);
    
    Msg.SetDeliveryMode(DELIVERY_MODE::RELIABLE);
    EXPECT_EQ(Msg.GetDeliveryMode(), DELIVERY_MODE::RELIABLE);
    
    Msg.SetSenderId(789);
    EXPECT_EQ(Msg.GetSenderId(), 789);
    
    Msg.SetReceiverId(101112);
    EXPECT_EQ(Msg.GetReceiverId(), 101112);
    
    Msg.SetTopicId(555);
    EXPECT_EQ(Msg.GetTopicId(), 555);
    
    Msg.SetSequenceNumber(42);
    EXPECT_EQ(Msg.GetSequenceNumber(), 42);
}

TEST_F(MessageTest, StaticFactoryMethodsWork)
{
    auto Msg1 = Message::Create(MESSAGE_TYPE::SERVICE_REGISTER);
    EXPECT_NE(Msg1, nullptr);
    EXPECT_EQ(Msg1->GetMessageType(), MESSAGE_TYPE::SERVICE_REGISTER);
    
    std::vector<uint8_t> TestData = {1, 2, 3, 4, 5};
    auto Msg2 = Message::Create(MESSAGE_TYPE::CUSTOM_MESSAGE_START, TestData);
    EXPECT_NE(Msg2, nullptr);
    EXPECT_EQ(Msg2->GetMessageType(), MESSAGE_TYPE::CUSTOM_MESSAGE_START);
    EXPECT_EQ(Msg2->GetPayloadSize(), TestData.size());
    
    std::string JsonPayload = R"({"key": "value"})";
    auto Msg3 = Message::Create(MESSAGE_TYPE::GAME_STATE_UPDATE, JsonPayload);
    EXPECT_NE(Msg3, nullptr);
    EXPECT_EQ(Msg3->GetJsonPayload(), JsonPayload);
}

TEST_F(MessageTest, CreateResponseWorksCorrectly)
{
    Message OriginalMsg(MESSAGE_TYPE::AUTH_LOGIN_REQUEST);
    OriginalMsg.SetSenderId(100);
    OriginalMsg.SetReceiverId(200);
    OriginalMsg.SetTopicId(300);
    
    auto ResponseMsg = Message::CreateResponse(OriginalMsg, MESSAGE_TYPE::AUTH_LOGIN_RESPONSE);
    
    EXPECT_NE(ResponseMsg, nullptr);
    EXPECT_EQ(ResponseMsg->GetMessageType(), MESSAGE_TYPE::AUTH_LOGIN_RESPONSE);
    EXPECT_EQ(ResponseMsg->GetSenderId(), OriginalMsg.GetReceiverId()); // Swapped
    EXPECT_EQ(ResponseMsg->GetReceiverId(), OriginalMsg.GetSenderId()); // Swapped
    EXPECT_EQ(ResponseMsg->GetTopicId(), OriginalMsg.GetTopicId());
}

TEST_F(MessageTest, ToStringWorksCorrectly)
{
    Message Msg(MESSAGE_TYPE::NETWORK_CONNECTION_ESTABLISHED);
    Msg.SetSenderId(123);
    Msg.SetReceiverId(456);
    Msg.SetPayload("Test payload");
    
    std::string MsgStr = Msg.ToString();
    std::string HeaderStr = Msg.GetHeaderString();
    
    // Basic checks that the strings contain expected content
    EXPECT_TRUE(MsgStr.find("Message{") != std::string::npos);
    EXPECT_TRUE(MsgStr.find("Id=") != std::string::npos);
    EXPECT_TRUE(MsgStr.find("Sender=123") != std::string::npos);
    EXPECT_TRUE(MsgStr.find("Receiver=456") != std::string::npos);
    
    EXPECT_TRUE(HeaderStr.find("MessageHeader{") != std::string::npos);
    EXPECT_TRUE(HeaderStr.find("SenderId=123") != std::string::npos);
    EXPECT_TRUE(HeaderStr.find("ReceiverId=456") != std::string::npos);
}

TEST_F(MessageTest, CopyAndMoveSemantics)
{
    Message Original(MESSAGE_TYPE::GAME_PLAYER_LEAVE);
    Original.SetPayload("Original payload");
    Original.SetSenderId(999);
    
    // Test copy constructor
    Message Copied(Original);
    EXPECT_EQ(Copied.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(Copied.GetJsonPayload(), Original.GetJsonPayload());
    EXPECT_EQ(Copied.GetSenderId(), Original.GetSenderId());
    
    // Test copy assignment
    Message CopyAssigned;
    CopyAssigned = Original;
    EXPECT_EQ(CopyAssigned.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(CopyAssigned.GetJsonPayload(), Original.GetJsonPayload());
    
    // Test move constructor
    Message Moved(std::move(Original));
    EXPECT_EQ(Moved.GetMessageType(), MESSAGE_TYPE::GAME_PLAYER_LEAVE);
    EXPECT_EQ(Moved.GetJsonPayload(), "Original payload");
    EXPECT_EQ(Moved.GetSenderId(), 999);
    
    // Test Clone method
    Message Clone = Moved.Clone();
    EXPECT_EQ(Clone.GetMessageType(), Moved.GetMessageType());
    EXPECT_EQ(Clone.GetJsonPayload(), Moved.GetJsonPayload());
    EXPECT_EQ(Clone.GetSenderId(), Moved.GetSenderId());
}

TEST_F(MessageTest, ResetWorksCorrectly)
{
    Message Msg(MESSAGE_TYPE::GAME_STATE_UPDATE);
    Msg.SetPayload("Some payload");
    Msg.SetSenderId(123);
    Msg.SetReceiverId(456);
    
    MessageId OriginalId = Msg.GetMessageId();
    
    Msg.Reset();
    
    // After reset, should have new ID and default values
    EXPECT_NE(Msg.GetMessageId(), OriginalId);
    EXPECT_EQ(Msg.GetSenderId(), 0);
    EXPECT_EQ(Msg.GetReceiverId(), 0);
    EXPECT_EQ(Msg.GetPayloadSize(), 0);
    EXPECT_FALSE(Msg.HasPayload());
    EXPECT_GT(Msg.GetTimestamp(), 0); // Should have new timestamp
}