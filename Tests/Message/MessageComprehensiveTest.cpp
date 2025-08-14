#include <gtest/gtest.h>
#include "Message/Message.h"
#include "Message/MessageTypes.h"
#include <vector>
#include <string>

using namespace Helianthus::Message;

class MessageComprehensiveTest : public ::testing::Test
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

TEST_F(MessageComprehensiveTest, DefaultConstructor)
{
    Message Msg;
    
    EXPECT_EQ(Msg.GetMessageId(), InvalidMessageId);
    EXPECT_EQ(Msg.GetMessageType(), MessageType::CUSTOM_MESSAGE_START);
    EXPECT_EQ(Msg.GetPriority(), MessagePriority::NORMAL);
    EXPECT_EQ(Msg.GetDeliveryMode(), DeliveryMode::FIRE_AND_FORGET);
    EXPECT_EQ(Msg.GetSenderId(), Common::InvalidServerId);
    EXPECT_EQ(Msg.GetReceiverId(), Common::InvalidServerId);
    EXPECT_EQ(Msg.GetTopicId(), InvalidTopicId);
    EXPECT_EQ(Msg.GetTimestamp(), 0);
    EXPECT_EQ(Msg.GetSequenceNumber(), 0);
    EXPECT_EQ(Msg.GetPayloadSize(), 0);
    EXPECT_FALSE(Msg.HasPayload());
}

TEST_F(MessageComprehensiveTest, ParameterizedConstructor)
{
    Message Msg(MessageType::GAME_PLAYER_JOIN);
    
    EXPECT_NE(Msg.GetMessageId(), InvalidMessageId);
    EXPECT_EQ(Msg.GetMessageType(), MessageType::GAME_PLAYER_JOIN);
    EXPECT_EQ(Msg.GetPriority(), MessagePriority::NORMAL);
    EXPECT_EQ(Msg.GetDeliveryMode(), DeliveryMode::FIRE_AND_FORGET);
    EXPECT_EQ(Msg.GetPayloadSize(), 0);
    EXPECT_FALSE(Msg.HasPayload());
    EXPECT_GT(Msg.GetTimestamp(), 0);
}

TEST_F(MessageComprehensiveTest, ConstructorWithPayload)
{
    std::string TestPayload = "Test payload data";
    Message Msg(MessageType::GAME_STATE_UPDATE, TestPayload);
    
    EXPECT_EQ(Msg.GetMessageType(), MessageType::GAME_STATE_UPDATE);
    EXPECT_EQ(Msg.GetPayloadSize(), TestPayload.size());
    EXPECT_TRUE(Msg.HasPayload());
    EXPECT_EQ(Msg.GetJsonPayload(), TestPayload);
}

TEST_F(MessageComprehensiveTest, ConstructorWithBinaryPayload)
{
    std::vector<uint8_t> BinaryPayload = {0x01, 0x02, 0x03, 0x04, 0x05};
    Message Msg(MessageType::NETWORK_DATA_RECEIVED, BinaryPayload);
    
    EXPECT_EQ(Msg.GetMessageType(), MessageType::NETWORK_DATA_RECEIVED);
    EXPECT_EQ(Msg.GetPayloadSize(), BinaryPayload.size());
    EXPECT_TRUE(Msg.HasPayload());
    EXPECT_EQ(Msg.GetPayload(), BinaryPayload);
}

TEST_F(MessageComprehensiveTest, CopyConstructor)
{
    Message Original(MessageType::AUTH_LOGIN_REQUEST);
    Original.SetPayload("Original payload");
    Original.SetSenderId(123);
    Original.SetReceiverId(456);
    Original.SetPriority(MessagePriority::HIGH);
    
    Message Copy(Original);
    
    EXPECT_EQ(Copy.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(Copy.GetPayload(), Original.GetPayload());
    EXPECT_EQ(Copy.GetSenderId(), Original.GetSenderId());
    EXPECT_EQ(Copy.GetReceiverId(), Original.GetReceiverId());
    EXPECT_EQ(Copy.GetPriority(), Original.GetPriority());
    EXPECT_EQ(Copy.GetMessageId(), Original.GetMessageId());
}

TEST_F(MessageComprehensiveTest, MoveConstructor)
{
    Message Original(MessageType::SYSTEM_HEARTBEAT);
    Original.SetPayload("Move test payload");
    auto OriginalId = Original.GetMessageId();
    auto OriginalPayload = Original.GetPayload();
    
    Message Moved(std::move(Original));
    
    EXPECT_EQ(Moved.GetMessageId(), OriginalId);
    EXPECT_EQ(Moved.GetPayload(), OriginalPayload);
    EXPECT_EQ(Moved.GetMessageType(), MessageType::SYSTEM_HEARTBEAT);
}

TEST_F(MessageComprehensiveTest, CopyAssignment)
{
    Message Original(MessageType::GAME_PLAYER_LEAVE);
    Original.SetPayload("Assignment test");
    Original.SetSenderId(789);
    
    Message Assigned;
    Assigned = Original;
    
    EXPECT_EQ(Assigned.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(Assigned.GetPayload(), Original.GetPayload());
    EXPECT_EQ(Assigned.GetSenderId(), Original.GetSenderId());
}

TEST_F(MessageComprehensiveTest, MoveAssignment)
{
    Message Original(MessageType::SERVICE_REGISTER);
    Original.SetPayload("Move assignment test");
    auto OriginalId = Original.GetMessageId();
    
    Message Assigned;
    Assigned = std::move(Original);
    
    EXPECT_EQ(Assigned.GetMessageId(), OriginalId);
    EXPECT_EQ(Assigned.GetMessageType(), MessageType::SERVICE_REGISTER);
}

TEST_F(MessageComprehensiveTest, SetAndGetPayload)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    
    // Test string payload
    std::string StringPayload = "String payload test";
    Msg.SetPayload(StringPayload);
    EXPECT_EQ(Msg.GetPayloadSize(), StringPayload.size());
    EXPECT_TRUE(Msg.HasPayload());
    EXPECT_EQ(Msg.GetJsonPayload(), StringPayload);
    
    // Test binary payload
    std::vector<uint8_t> BinaryPayload = {0xAA, 0xBB, 0xCC, 0xDD};
    Msg.SetPayload(BinaryPayload);
    EXPECT_EQ(Msg.GetPayloadSize(), BinaryPayload.size());
    EXPECT_EQ(Msg.GetPayload(), BinaryPayload);
    
    // Test raw data payload
    uint8_t RawData[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    Msg.SetPayload(RawData, sizeof(RawData));
    EXPECT_EQ(Msg.GetPayloadSize(), sizeof(RawData));
}

TEST_F(MessageComprehensiveTest, SetAndGetJsonPayload)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    
    std::string JsonPayload = "{\"key\": \"value\", \"number\": 42}";
    EXPECT_TRUE(Msg.SetJsonPayload(JsonPayload));
    EXPECT_EQ(Msg.GetJsonPayload(), JsonPayload);
    
    // Test invalid JSON
    std::string InvalidJson = "{invalid json}";
    EXPECT_FALSE(Msg.SetJsonPayload(InvalidJson));
}

TEST_F(MessageComprehensiveTest, MessageProperties)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    
    // Test all property setters and getters
    Msg.SetMessageId(12345);
    EXPECT_EQ(Msg.GetMessageId(), 12345);
    
    Msg.SetMessageType(MessageType::AUTH_LOGIN_RESPONSE);
    EXPECT_EQ(Msg.GetMessageType(), MessageType::AUTH_LOGIN_RESPONSE);
    
    Msg.SetPriority(MessagePriority::CRITICAL);
    EXPECT_EQ(Msg.GetPriority(), MessagePriority::CRITICAL);
    
    Msg.SetDeliveryMode(DeliveryMode::RELIABLE);
    EXPECT_EQ(Msg.GetDeliveryMode(), DeliveryMode::RELIABLE);
    
    Msg.SetSenderId(100);
    EXPECT_EQ(Msg.GetSenderId(), 100);
    
    Msg.SetReceiverId(200);
    EXPECT_EQ(Msg.GetReceiverId(), 200);
    
    Msg.SetTopicId(300);
    EXPECT_EQ(Msg.GetTopicId(), 300);
    
    Msg.SetTimestamp(1234567890);
    EXPECT_EQ(Msg.GetTimestamp(), 1234567890);
    
    Msg.SetSequenceNumber(42);
    EXPECT_EQ(Msg.GetSequenceNumber(), 42);
}

TEST_F(MessageComprehensiveTest, SerializationAndDeserialization)
{
    Message Original(MessageType::GAME_STATE_UPDATE);
    Original.SetPayload("Serialization test payload");
    Original.SetSenderId(123);
    Original.SetReceiverId(456);
    Original.SetPriority(MessagePriority::HIGH);
    Original.SetDeliveryMode(DeliveryMode::ORDERED);
    Original.SetTopicId(789);
    Original.SetTimestamp(987654321);
    Original.SetSequenceNumber(999);
    
    // Serialize
    auto SerializedData = Original.Serialize();
    EXPECT_GT(SerializedData.size(), 0);
    
    // Deserialize
    Message Deserialized;
    bool DeserializeResult = Deserialized.Deserialize(SerializedData);
    
    EXPECT_TRUE(DeserializeResult);
    EXPECT_EQ(Deserialized.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(Deserialized.GetPayload(), Original.GetPayload());
    EXPECT_EQ(Deserialized.GetSenderId(), Original.GetSenderId());
    EXPECT_EQ(Deserialized.GetReceiverId(), Original.GetReceiverId());
    EXPECT_EQ(Deserialized.GetPriority(), Original.GetPriority());
    EXPECT_EQ(Deserialized.GetDeliveryMode(), Original.GetDeliveryMode());
    EXPECT_EQ(Deserialized.GetTopicId(), Original.GetTopicId());
    EXPECT_EQ(Deserialized.GetTimestamp(), Original.GetTimestamp());
    EXPECT_EQ(Deserialized.GetSequenceNumber(), Original.GetSequenceNumber());
}

TEST_F(MessageComprehensiveTest, DeserializationWithRawData)
{
    Message Original(MessageType::NETWORK_DATA_RECEIVED);
    Original.SetPayload("Raw data test");
    
    auto SerializedData = Original.Serialize();
    
    Message Deserialized;
    bool Result = Deserialized.Deserialize(SerializedData.data(), SerializedData.size());
    
    EXPECT_TRUE(Result);
    EXPECT_EQ(Deserialized.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(Deserialized.GetPayload(), Original.GetPayload());
}

TEST_F(MessageComprehensiveTest, ChecksumValidation)
{
    Message Msg(MessageType::SYSTEM_HEARTBEAT);
    Msg.SetPayload("Checksum validation test");
    
    // Update checksum
    Msg.UpdateChecksum();
    EXPECT_TRUE(Msg.ValidateChecksum());
    
    // Manually corrupt checksum
    auto& Header = Msg.GetHeader();
    uint32_t OriginalChecksum = Header.Checksum;
    Header.Checksum = 0xDEADBEEF;
    EXPECT_FALSE(Msg.ValidateChecksum());
    
    // Restore checksum
    Header.Checksum = OriginalChecksum;
    EXPECT_TRUE(Msg.ValidateChecksum());
}

TEST_F(MessageComprehensiveTest, MessageValidation)
{
    Message ValidMsg(MessageType::GAME_PLAYER_JOIN);
    ValidMsg.SetPayload("Valid message");
    EXPECT_TRUE(ValidMsg.IsValid());
    
    Message InvalidMsg;
    auto& InvalidHeader = InvalidMsg.GetHeader();
    InvalidHeader.MsgId = InvalidMessageId;
    EXPECT_FALSE(InvalidMsg.IsValid());
}

TEST_F(MessageComprehensiveTest, CalculateChecksum)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    Msg.SetPayload("Checksum calculation test");
    
    uint32_t Checksum1 = Msg.CalculateChecksum();
    uint32_t Checksum2 = Msg.CalculateChecksum();
    
    EXPECT_EQ(Checksum1, Checksum2);
    EXPECT_NE(Checksum1, 0);
}

TEST_F(MessageComprehensiveTest, GetTotalSize)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    EXPECT_EQ(Msg.GetTotalSize(), sizeof(MessageHeader));
    
    Msg.SetPayload("Test payload");
    EXPECT_EQ(Msg.GetTotalSize(), sizeof(MessageHeader) + Msg.GetPayloadSize());
}

TEST_F(MessageComprehensiveTest, Reset)
{
    Message Msg(MessageType::GAME_STATE_UPDATE);
    Msg.SetPayload("Reset test payload");
    Msg.SetSenderId(123);
    Msg.SetReceiverId(456);
    
    Msg.Reset();
    
    EXPECT_EQ(Msg.GetMessageId(), InvalidMessageId);
    EXPECT_EQ(Msg.GetMessageType(), MessageType::CUSTOM_MESSAGE_START);
    EXPECT_EQ(Msg.GetPayloadSize(), 0);
    EXPECT_FALSE(Msg.HasPayload());
    EXPECT_EQ(Msg.GetSenderId(), Common::InvalidServerId);
    EXPECT_EQ(Msg.GetReceiverId(), Common::InvalidServerId);
}

TEST_F(MessageComprehensiveTest, Clone)
{
    Message Original(MessageType::AUTH_LOGIN_REQUEST);
    Original.SetPayload("Clone test payload");
    Original.SetSenderId(111);
    Original.SetReceiverId(222);
    Original.SetPriority(MessagePriority::HIGH);
    
    Message Cloned = Original.Clone();
    
    EXPECT_EQ(Cloned.GetMessageType(), Original.GetMessageType());
    EXPECT_EQ(Cloned.GetPayload(), Original.GetPayload());
    EXPECT_EQ(Cloned.GetSenderId(), Original.GetSenderId());
    EXPECT_EQ(Cloned.GetReceiverId(), Original.GetReceiverId());
    EXPECT_EQ(Cloned.GetPriority(), Original.GetPriority());
    
    // Clone should have a different message ID
    EXPECT_NE(Cloned.GetMessageId(), Original.GetMessageId());
}

TEST_F(MessageComprehensiveTest, ToString)
{
    Message Msg(MessageType::GAME_PLAYER_JOIN);
    Msg.SetPayload("ToString test");
    Msg.SetSenderId(123);
    Msg.SetReceiverId(456);
    
    std::string StringRep = Msg.ToString();
    EXPECT_FALSE(StringRep.empty());
    EXPECT_NE(StringRep.find("GAME_PLAYER_JOIN"), std::string::npos);
}

TEST_F(MessageComprehensiveTest, GetHeaderString)
{
    Message Msg(MessageType::SYSTEM_STATUS);
    Msg.SetSenderId(789);
    Msg.SetReceiverId(101);
    
    std::string HeaderString = Msg.GetHeaderString();
    EXPECT_FALSE(HeaderString.empty());
    EXPECT_NE(HeaderString.find("SYSTEM_STATUS"), std::string::npos);
}

TEST_F(MessageComprehensiveTest, LargePayload)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    
    // Create a large payload
    std::string LargePayload(10000, 'A');
    Msg.SetPayload(LargePayload);
    
    EXPECT_EQ(Msg.GetPayloadSize(), 10000);
    EXPECT_TRUE(Msg.HasPayload());
    
    // Test serialization with large payload
    auto SerializedData = Msg.Serialize();
    EXPECT_GT(SerializedData.size(), 10000);
    
    // Test deserialization
    Message Deserialized;
    bool Result = Deserialized.Deserialize(SerializedData);
    EXPECT_TRUE(Result);
    EXPECT_EQ(Deserialized.GetPayload(), LargePayload);
}

TEST_F(MessageComprehensiveTest, EmptyPayload)
{
    Message Msg(MessageType::CUSTOM_MESSAGE_START);
    
    // Set empty payload
    Msg.SetPayload("");
    EXPECT_EQ(Msg.GetPayloadSize(), 0);
    EXPECT_FALSE(Msg.HasPayload());
    
    // Serialize and deserialize
    auto SerializedData = Msg.Serialize();
    Message Deserialized;
    bool Result = Deserialized.Deserialize(SerializedData);
    EXPECT_TRUE(Result);
    EXPECT_EQ(Deserialized.GetPayloadSize(), 0);
    EXPECT_FALSE(Deserialized.HasPayload());
}

TEST_F(MessageComprehensiveTest, BinaryPayloadWithNullBytes)
{
    Message Msg(MessageType::NETWORK_DATA_RECEIVED);
    
    // Create payload with null bytes
    std::vector<uint8_t> BinaryPayload = {0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
    Msg.SetPayload(BinaryPayload);
    
    EXPECT_EQ(Msg.GetPayloadSize(), BinaryPayload.size());
    EXPECT_EQ(Msg.GetPayload(), BinaryPayload);
    
    // Test serialization
    auto SerializedData = Msg.Serialize();
    Message Deserialized;
    bool Result = Deserialized.Deserialize(SerializedData);
    EXPECT_TRUE(Result);
    EXPECT_EQ(Deserialized.GetPayload(), BinaryPayload);
}
