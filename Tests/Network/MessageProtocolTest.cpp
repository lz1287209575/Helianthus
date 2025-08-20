#include "Shared/Network/Asio/MessageProtocol.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;

class MessageProtocolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Protocol = std::make_unique<MessageProtocol>();
        ReceivedMessages.clear();

        Protocol->SetMessageHandler([this](const std::string& Message)
                                    { ReceivedMessages.push_back(Message); });
    }

    std::unique_ptr<MessageProtocol> Protocol;
    std::vector<std::string> ReceivedMessages;
};

TEST_F(MessageProtocolTest, EncodeDecodeMessage)
{
    // 测试编码和解码单个消息
    std::string TestMessage = "Hello, World!";
    auto Encoded = MessageProtocol::EncodeMessage(TestMessage);

    // 编码的消息应该包含4字节长度头 + 消息内容
    EXPECT_EQ(Encoded.size(), sizeof(uint32_t) + TestMessage.size());

    // 处理编码的数据
    Protocol->ProcessReceivedData(Encoded.data(), Encoded.size());

    // 应该收到一条消息
    ASSERT_EQ(ReceivedMessages.size(), 1);
    EXPECT_EQ(ReceivedMessages[0], TestMessage);
}

TEST_F(MessageProtocolTest, HandleMultipleMessages)
{
    // 测试处理多个消息
    std::vector<std::string> TestMessages = {
        "Message 1", "This is a longer message", "Short", "Another message with different length"};

    // 编码所有消息并拼接
    std::vector<char> CombinedData;
    for (const auto& Msg : TestMessages)
    {
        auto Encoded = MessageProtocol::EncodeMessage(Msg);
        CombinedData.insert(CombinedData.end(), Encoded.begin(), Encoded.end());
    }

    // 一次性处理所有数据
    Protocol->ProcessReceivedData(CombinedData.data(), CombinedData.size());

    // 应该收到所有消息
    ASSERT_EQ(ReceivedMessages.size(), TestMessages.size());
    for (size_t i = 0; i < TestMessages.size(); ++i)
    {
        EXPECT_EQ(ReceivedMessages[i], TestMessages[i]);
    }
}

TEST_F(MessageProtocolTest, HandleFragmentedData)
{
    // 测试处理分片数据（模拟半包）
    std::string TestMessage = "This is a test message for fragmentation";
    auto Encoded = MessageProtocol::EncodeMessage(TestMessage);

    // 分片发送数据
    size_t ChunkSize = 5;
    size_t Offset = 0;

    while (Offset < Encoded.size())
    {
        size_t CurrentChunkSize = std::min(ChunkSize, Encoded.size() - Offset);
        Protocol->ProcessReceivedData(Encoded.data() + Offset, CurrentChunkSize);
        Offset += CurrentChunkSize;
    }

    // 应该收到完整的消息
    ASSERT_EQ(ReceivedMessages.size(), 1);
    EXPECT_EQ(ReceivedMessages[0], TestMessage);
}

TEST_F(MessageProtocolTest, HandlePartialLength)
{
    // 测试处理部分长度头的情况
    std::string TestMessage = "Test message";
    auto Encoded = MessageProtocol::EncodeMessage(TestMessage);

    // 首先只发送2字节的长度头
    Protocol->ProcessReceivedData(Encoded.data(), 2);
    EXPECT_EQ(ReceivedMessages.size(), 0);  // 不应该有消息

    // 发送剩余的长度头
    Protocol->ProcessReceivedData(Encoded.data() + 2, 2);
    EXPECT_EQ(ReceivedMessages.size(), 0);  // 仍然不应该有消息

    // 发送消息内容
    Protocol->ProcessReceivedData(Encoded.data() + 4, Encoded.size() - 4);

    // 现在应该收到完整的消息
    ASSERT_EQ(ReceivedMessages.size(), 1);
    EXPECT_EQ(ReceivedMessages[0], TestMessage);
}

TEST_F(MessageProtocolTest, HandleEmptyMessage)
{
    // 测试处理空消息
    std::string EmptyMessage = "";
    auto Encoded = MessageProtocol::EncodeMessage(EmptyMessage);

    Protocol->ProcessReceivedData(Encoded.data(), Encoded.size());

    ASSERT_EQ(ReceivedMessages.size(), 1);
    EXPECT_EQ(ReceivedMessages[0], EmptyMessage);
}

TEST_F(MessageProtocolTest, ResetProtocol)
{
    // 测试重置协议状态
    std::string TestMessage = "Test message";
    auto Encoded = MessageProtocol::EncodeMessage(TestMessage);

    // 只发送一部分数据
    Protocol->ProcessReceivedData(Encoded.data(), 2);
    EXPECT_GT(Protocol->GetBufferSize(), 0);

    // 重置协议
    Protocol->Reset();
    EXPECT_EQ(Protocol->GetBufferSize(), 0);

    // 现在发送完整消息应该正常工作
    Protocol->ProcessReceivedData(Encoded.data(), Encoded.size());
    ASSERT_EQ(ReceivedMessages.size(), 1);
    EXPECT_EQ(ReceivedMessages[0], TestMessage);
}
