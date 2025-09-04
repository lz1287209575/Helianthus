#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

class MessageQueueCompressionTest : public ::testing::Test
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

TEST_F(MessageQueueCompressionTest, GzipCompressionWorks)
{
    // 创建测试队列
    std::string QueueName = "compression_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 配置压缩
    Helianthus::MessageQueue::CompressionConfig CompConfig;
    CompConfig.Algorithm = Helianthus::MessageQueue::CompressionAlgorithm::GZIP;
    CompConfig.Level = 6;
    CompConfig.MinSize = 100;
    CompConfig.EnableAutoCompression = true;

    Queue->SetCompressionConfig(QueueName, CompConfig);

    // 创建大消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    std::string LargePayload(1000, 'A');  // 1KB 重复数据，应该压缩得很好
    Message->Payload.Data.assign(LargePayload.begin(), LargePayload.end());
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

    // 发送消息
    auto Result = Queue->SendMessage(QueueName, Message);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    std::string ReceivedPayload(ReceivedMessage->Payload.Data.begin(),
                                ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedPayload, LargePayload);
}

TEST_F(MessageQueueCompressionTest, Aes128CbcEncryptionWorks)
{
    // 创建测试队列
    std::string QueueName = "encryption_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 配置加密
    Helianthus::MessageQueue::EncryptionConfig EncConfig;
    EncConfig.Algorithm = Helianthus::MessageQueue::EncryptionAlgorithm::AES_128_CBC;
    EncConfig.Key = "MySecretKey12345";  // 16字节密钥
    EncConfig.IV = "MyIV1234567890123";  // 16字节IV
    EncConfig.EnableAutoEncryption = true;

    Queue->SetEncryptionConfig(QueueName, EncConfig);

    // 创建消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    std::string Payload = "This is a secret message that should be encrypted";
    Message->Payload.Data.assign(Payload.begin(), Payload.end());
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

    // 发送消息
    auto Result = Queue->SendMessage(QueueName, Message);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    std::string ReceivedPayload(ReceivedMessage->Payload.Data.begin(),
                                ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedPayload, Payload);
}

TEST_F(MessageQueueCompressionTest, Aes256GcmEncryptionWorks)
{
    // 创建测试队列
    std::string QueueName = "gcm_encryption_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 配置加密
    Helianthus::MessageQueue::EncryptionConfig EncConfig;
    EncConfig.Algorithm = Helianthus::MessageQueue::EncryptionAlgorithm::AES_256_GCM;
    EncConfig.Key = "MySecretKey123456789012345678901234";  // 32字节密钥
    EncConfig.IV = "MyIV123456789";                         // 12字节IV
    EncConfig.EnableAutoEncryption = true;

    Queue->SetEncryptionConfig(QueueName, EncConfig);

    // 创建消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    std::string Payload = "This is a secret message that should be encrypted with GCM";
    Message->Payload.Data.assign(Payload.begin(), Payload.end());
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

    // 发送消息
    auto Result = Queue->SendMessage(QueueName, Message);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    std::string ReceivedPayload(ReceivedMessage->Payload.Data.begin(),
                                ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedPayload, Payload);
}

TEST_F(MessageQueueCompressionTest, CompressionAndEncryptionCombined)
{
    // 创建测试队列
    std::string QueueName = "combined_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 配置压缩
    Helianthus::MessageQueue::CompressionConfig CompConfig;
    CompConfig.Algorithm = Helianthus::MessageQueue::CompressionAlgorithm::GZIP;
    CompConfig.Level = 6;
    CompConfig.MinSize = 100;
    CompConfig.EnableAutoCompression = true;

    // 配置加密
    Helianthus::MessageQueue::EncryptionConfig EncConfig;
    EncConfig.Algorithm = Helianthus::MessageQueue::EncryptionAlgorithm::AES_128_CBC;
    EncConfig.Key = "MySecretKey12345";
    EncConfig.IV = "MyIV1234567890123";
    EncConfig.EnableAutoEncryption = true;

    Queue->SetCompressionConfig(QueueName, CompConfig);
    Queue->SetEncryptionConfig(QueueName, EncConfig);

    // 创建大消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    std::string LargePayload(2000, 'A');  // 2KB 重复数据
    Message->Payload.Data.assign(LargePayload.begin(), LargePayload.end());
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

    // 发送消息
    auto Result = Queue->SendMessage(QueueName, Message);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);

    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_NE(ReceivedMessage, nullptr);
    std::string ReceivedPayload(ReceivedMessage->Payload.Data.begin(),
                                ReceivedMessage->Payload.Data.end());
    EXPECT_EQ(ReceivedPayload, LargePayload);
}

TEST_F(MessageQueueCompressionTest, CompressionStatsAreUpdated)
{
    // 创建测试队列
    std::string QueueName = "stats_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 配置压缩
    Helianthus::MessageQueue::CompressionConfig CompConfig;
    CompConfig.Algorithm = Helianthus::MessageQueue::CompressionAlgorithm::GZIP;
    CompConfig.Level = 6;
    CompConfig.MinSize = 100;
    CompConfig.EnableAutoCompression = true;

    Queue->SetCompressionConfig(QueueName, CompConfig);

    // 发送多个消息
    for (int i = 0; i < 5; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload(500, 'A' + i);  // 不同内容的大消息
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

        auto Result = Queue->SendMessage(QueueName, Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 接收所有消息
    for (int i = 0; i < 5; ++i)
    {
        Helianthus::MessageQueue::MessagePtr ReceivedMessage;
        auto Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }

    // 检查压缩统计
    Helianthus::MessageQueue::CompressionStats CompStats;
    auto Result = Queue->GetCompressionStats(QueueName, CompStats);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_GT(CompStats.CompressedMessages, 0);
    EXPECT_GT(CompStats.CompressionRatio, 0.0);
}

TEST_F(MessageQueueCompressionTest, EncryptionStatsAreUpdated)
{
    // 创建测试队列
    std::string QueueName = "encryption_stats_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Queue->CreateQueue(Config);

    // 配置加密
    Helianthus::MessageQueue::EncryptionConfig EncConfig;
    EncConfig.Algorithm = Helianthus::MessageQueue::EncryptionAlgorithm::AES_128_CBC;
    EncConfig.Key = "MySecretKey12345";
    EncConfig.IV = "MyIV1234567890123";
    EncConfig.EnableAutoEncryption = true;

    Queue->SetEncryptionConfig(QueueName, EncConfig);

    // 发送多个消息
    for (int i = 0; i < 3; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = "Secret message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;

        auto Result = Queue->SendMessage(QueueName, Message);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    }

    // 接收所有消息
    for (int i = 0; i < 3; ++i)
    {
        Helianthus::MessageQueue::MessagePtr ReceivedMessage;
        auto Result = Queue->ReceiveMessage(QueueName, ReceivedMessage);
        EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
        EXPECT_NE(ReceivedMessage, nullptr);
    }

    // 检查加密统计
    Helianthus::MessageQueue::EncryptionStats EncStats;
    auto Result = Queue->GetEncryptionStats(QueueName, EncStats);
    EXPECT_EQ(Result, Helianthus::MessageQueue::QueueResult::SUCCESS);
    EXPECT_GT(EncStats.EncryptedMessages, 0);
    EXPECT_GT(EncStats.AverageEncryptionTimeMs, 0.0);
}
