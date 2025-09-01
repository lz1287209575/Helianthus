#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"

class PerformanceBenchmarkTest : public ::testing::Test
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

    // 生成随机数据
    std::string GenerateRandomData(size_t Size)
    {
        std::random_device Rd;
        std::mt19937 Gen(Rd());
        std::uniform_int_distribution<> Dis(32, 126); // 可打印ASCII字符
        
        std::string Data;
        Data.reserve(Size);
        for (size_t i = 0; i < Size; ++i)
        {
            Data.push_back(static_cast<char>(Dis(Gen)));
        }
        return Data;
    }

    // 生成可压缩数据（重复模式）
    std::string GenerateCompressibleData(size_t Size)
    {
        std::string Pattern = "This is a repeated pattern that should compress well ";
        std::string Data;
        Data.reserve(Size);
        
        while (Data.size() < Size)
        {
            Data += Pattern;
        }
        Data.resize(Size);
        return Data;
    }

    // 性能测试辅助函数
    template<typename FuncType>
    double MeasureTime(FuncType Func)
    {
        auto Start = std::chrono::high_resolution_clock::now();
        Func();
        auto End = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(End - Start);
        return Duration.count() / 1000.0; // 转换为毫秒
    }

    // 计算吞吐量（消息/秒）
    double CalculateThroughput(int MessageCount, double TotalTimeMs)
    {
        return (MessageCount * 1000.0) / TotalTimeMs;
    }

    // 计算压缩率
    double CalculateCompressionRatio(size_t OriginalSize, size_t CompressedSize)
    {
        return (1.0 - static_cast<double>(CompressedSize) / OriginalSize) * 100.0;
    }

    std::unique_ptr<Helianthus::MessageQueue::MessageQueue> Queue;
};

// 基础消息发送接收性能测试
TEST_F(PerformanceBenchmarkTest, BasicMessageThroughput)
{
    std::string QueueName = "basic_perf_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    const int MessageCount = 10000;
    const size_t MessageSize = 1024; // 1KB
    
    std::vector<std::shared_ptr<Helianthus::MessageQueue::Message>> Messages;
    Messages.reserve(MessageCount);

    // 准备消息
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = GenerateRandomData(MessageSize);
        Msg->Payload.Data.assign(Payload.begin(), Payload.end());
        Msg->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        Messages.push_back(Msg);
    }

    // 测试发送性能
    double SendTime = MeasureTime([&]() {
        for (const auto& Msg : Messages)
        {
            Queue->SendMessage(QueueName, Msg);
        }
    });

    // 测试接收性能
    double ReceiveTime = MeasureTime([&]() {
        for (int i = 0; i < MessageCount; ++i)
        {
            Helianthus::MessageQueue::MessagePtr ReceivedMsg;
            Queue->ReceiveMessage(QueueName, ReceivedMsg);
        }
    });

    double SendThroughput = CalculateThroughput(MessageCount, SendTime);
    double ReceiveThroughput = CalculateThroughput(MessageCount, ReceiveTime);

    std::cout << "\n=== 基础消息性能测试 ===" << std::endl;
    std::cout << "消息数量: " << MessageCount << std::endl;
    std::cout << "消息大小: " << MessageSize << " 字节" << std::endl;
    std::cout << "发送时间: " << std::fixed << std::setprecision(2) << SendTime << " ms" << std::endl;
    std::cout << "接收时间: " << ReceiveTime << " ms" << std::endl;
    std::cout << "发送吞吐量: " << SendThroughput << " 消息/秒" << std::endl;
    std::cout << "接收吞吐量: " << ReceiveThroughput << " 消息/秒" << std::endl;

    // 验证性能基准
    EXPECT_GT(SendThroughput, 1000); // 至少1000消息/秒
    EXPECT_GT(ReceiveThroughput, 1000);
}

// 压缩性能测试
TEST_F(PerformanceBenchmarkTest, CompressionPerformance)
{
    std::string QueueName = "compression_perf_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 配置压缩
    Helianthus::MessageQueue::CompressionConfig CompConfig;
    CompConfig.Algorithm = Helianthus::MessageQueue::CompressionAlgorithm::GZIP;
    CompConfig.Level = 6;
    CompConfig.MinSize = 100;
    CompConfig.EnableAutoCompression = true;
    Queue->SetCompressionConfig(QueueName, CompConfig);

    const int MessageCount = 1000;
    const size_t MessageSize = 4096; // 4KB
    
    std::vector<std::shared_ptr<Helianthus::MessageQueue::Message>> Messages;
    Messages.reserve(MessageCount);

    // 准备可压缩的消息
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = GenerateCompressibleData(MessageSize);
        Msg->Payload.Data.assign(Payload.begin(), Payload.end());
        Msg->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        Messages.push_back(Msg);
    }

    // 测试无压缩性能
    double NoCompressionTime = MeasureTime([&]() {
        for (const auto& Msg : Messages)
        {
            Queue->SendMessage(QueueName, Msg);
        }
    });

    // 重新配置队列，启用压缩
    Queue->DeleteQueue(QueueName);
    Queue->CreateQueue(Config);
    Queue->SetCompressionConfig(QueueName, CompConfig);

    // 测试压缩性能
    double CompressionTime = MeasureTime([&]() {
        for (const auto& Msg : Messages)
        {
            Queue->SendMessage(QueueName, Msg);
        }
    });

    double CompressionOverhead = ((CompressionTime - NoCompressionTime) / NoCompressionTime) * 100.0;

    std::cout << "\n=== 压缩性能测试 ===" << std::endl;
    std::cout << "消息数量: " << MessageCount << std::endl;
    std::cout << "消息大小: " << MessageSize << " 字节" << std::endl;
    std::cout << "无压缩时间: " << std::fixed << std::setprecision(2) << NoCompressionTime << " ms" << std::endl;
    std::cout << "压缩时间: " << CompressionTime << " ms" << std::endl;
    std::cout << "压缩开销: " << CompressionOverhead << "%" << std::endl;

    // 验证压缩性能
    EXPECT_LT(CompressionOverhead, 50.0); // 压缩开销应小于50%
}

// 加密性能测试
TEST_F(PerformanceBenchmarkTest, EncryptionPerformance)
{
    std::string QueueName = "encryption_perf_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 配置加密
    Helianthus::MessageQueue::EncryptionConfig EncConfig;
    EncConfig.Algorithm = Helianthus::MessageQueue::EncryptionAlgorithm::AES_128_CBC;
    EncConfig.Key = "MySecretKey12345";
    EncConfig.IV = "MyIV1234567890123";
    EncConfig.EnableAutoEncryption = true;
    Queue->SetEncryptionConfig(QueueName, EncConfig);

    const int MessageCount = 1000;
    const size_t MessageSize = 1024; // 1KB
    
    std::vector<std::shared_ptr<Helianthus::MessageQueue::Message>> Messages;
    Messages.reserve(MessageCount);

    // 准备消息
    for (int i = 0; i < MessageCount; ++i)
    {
        auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = GenerateRandomData(MessageSize);
        Msg->Payload.Data.assign(Payload.begin(), Payload.end());
        Msg->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        Messages.push_back(Msg);
    }

    // 测试无加密性能
    double NoEncryptionTime = MeasureTime([&]() {
        for (const auto& Msg : Messages)
        {
            Queue->SendMessage(QueueName, Msg);
        }
    });

    // 重新配置队列，启用加密
    Queue->DeleteQueue(QueueName);
    Queue->CreateQueue(Config);
    Queue->SetEncryptionConfig(QueueName, EncConfig);

    // 测试加密性能
    double EncryptionTime = MeasureTime([&]() {
        for (const auto& Msg : Messages)
        {
            Queue->SendMessage(QueueName, Msg);
        }
    });

    double EncryptionOverhead = ((EncryptionTime - NoEncryptionTime) / NoEncryptionTime) * 100.0;

    std::cout << "\n=== 加密性能测试 ===" << std::endl;
    std::cout << "消息数量: " << MessageCount << std::endl;
    std::cout << "消息大小: " << MessageSize << " 字节" << std::endl;
    std::cout << "无加密时间: " << std::fixed << std::setprecision(2) << NoEncryptionTime << " ms" << std::endl;
    std::cout << "加密时间: " << EncryptionTime << " ms" << std::endl;
    std::cout << "加密开销: " << EncryptionOverhead << "%" << std::endl;

    // 验证加密性能
    EXPECT_LT(EncryptionOverhead, 100.0); // 加密开销应小于100%
}

// 批处理性能测试
TEST_F(PerformanceBenchmarkTest, BatchProcessingPerformance)
{
    std::string QueueName = "batch_perf_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    const int BatchCount = 100;
    const int MessagesPerBatch = 100;
    const size_t MessageSize = 512; // 512字节
    
    std::vector<std::shared_ptr<Helianthus::MessageQueue::Message>> AllMessages;
    AllMessages.reserve(BatchCount * MessagesPerBatch);

    // 准备所有消息
    for (int i = 0; i < BatchCount * MessagesPerBatch; ++i)
    {
        auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
        std::string Payload = GenerateRandomData(MessageSize);
        Msg->Payload.Data.assign(Payload.begin(), Payload.end());
        Msg->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        AllMessages.push_back(Msg);
    }

    // 测试单个消息发送性能
    double SingleMessageTime = MeasureTime([&]() {
        for (const auto& Msg : AllMessages)
        {
            Queue->SendMessage(QueueName, Msg);
        }
    });

    // 测试批处理性能
    double BatchTime = MeasureTime([&]() {
        for (int b = 0; b < BatchCount; ++b)
        {
            uint32_t BatchId = 0;
            Queue->CreateBatchForQueue(QueueName, BatchId);
            
            for (int m = 0; m < MessagesPerBatch; ++m)
            {
                int Index = b * MessagesPerBatch + m;
                Queue->AddToBatch(BatchId, AllMessages[Index]);
            }
            
            Queue->CommitBatch(BatchId);
        }
    });

    double BatchSpeedup = SingleMessageTime / BatchTime;
    double BatchThroughput = CalculateThroughput(BatchCount * MessagesPerBatch, BatchTime);

    std::cout << "\n=== 批处理性能测试 ===" << std::endl;
    std::cout << "批次数: " << BatchCount << std::endl;
    std::cout << "每批消息数: " << MessagesPerBatch << std::endl;
    std::cout << "总消息数: " << BatchCount * MessagesPerBatch << std::endl;
    std::cout << "消息大小: " << MessageSize << " 字节" << std::endl;
    std::cout << "单个消息时间: " << std::fixed << std::setprecision(2) << SingleMessageTime << " ms" << std::endl;
    std::cout << "批处理时间: " << BatchTime << " ms" << std::endl;
    std::cout << "批处理加速比: " << BatchSpeedup << "x" << std::endl;
    std::cout << "批处理吞吐量: " << BatchThroughput << " 消息/秒" << std::endl;

    // 验证批处理性能
    EXPECT_GT(BatchSpeedup, 1.0); // 批处理应该比单个消息快
    EXPECT_GT(BatchThroughput, 5000); // 至少5000消息/秒
}

// 零拷贝性能测试
TEST_F(PerformanceBenchmarkTest, ZeroCopyPerformance)
{
    std::string QueueName = "zerocopy_perf_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    const int OperationCount = 5000;
    const size_t DataSize = 2048; // 2KB
    
    std::vector<std::string> DataChunks;
    DataChunks.reserve(OperationCount);

    // 准备数据
    for (int i = 0; i < OperationCount; ++i)
    {
        DataChunks.push_back(GenerateRandomData(DataSize));
    }

    // 测试普通发送性能
    double NormalTime = MeasureTime([&]() {
        for (const auto& Data : DataChunks)
        {
            auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
            Msg->Payload.Data.assign(Data.begin(), Data.end());
            Msg->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
            Queue->SendMessage(QueueName, Msg);
        }
    });

    // 测试零拷贝性能
    double ZeroCopyTime = MeasureTime([&]() {
        for (const auto& Data : DataChunks)
        {
            Helianthus::MessageQueue::ZeroCopyBuffer ZB{};
            Queue->CreateZeroCopyBuffer(Data.c_str(), Data.size(), ZB);
            Queue->SendMessageZeroCopy(QueueName, ZB);
            Queue->ReleaseZeroCopyBuffer(ZB);
        }
    });

    double ZeroCopySpeedup = NormalTime / ZeroCopyTime;
    double ZeroCopyThroughput = CalculateThroughput(OperationCount, ZeroCopyTime);

    std::cout << "\n=== 零拷贝性能测试 ===" << std::endl;
    std::cout << "操作次数: " << OperationCount << std::endl;
    std::cout << "数据大小: " << DataSize << " 字节" << std::endl;
    std::cout << "普通发送时间: " << std::fixed << std::setprecision(2) << NormalTime << " ms" << std::endl;
    std::cout << "零拷贝时间: " << ZeroCopyTime << " ms" << std::endl;
    std::cout << "零拷贝加速比: " << ZeroCopySpeedup << "x" << std::endl;
    std::cout << "零拷贝吞吐量: " << ZeroCopyThroughput << " 操作/秒" << std::endl;

    // 验证零拷贝性能
    EXPECT_GT(ZeroCopyThroughput, 2000); // 至少2000操作/秒
    // 注意：零拷贝在小数据量时可能不会比普通发送快，这是正常的
    // 零拷贝的优势主要体现在大数据量和减少内存拷贝上
}

// 综合性能测试
TEST_F(PerformanceBenchmarkTest, ComprehensivePerformance)
{
    std::string QueueName = "comprehensive_perf_test";
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = QueueName;
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);

    // 配置压缩和加密
    Helianthus::MessageQueue::CompressionConfig CompConfig;
    CompConfig.Algorithm = Helianthus::MessageQueue::CompressionAlgorithm::GZIP;
    CompConfig.Level = 6;
    CompConfig.MinSize = 100;
    CompConfig.EnableAutoCompression = true;

    Helianthus::MessageQueue::EncryptionConfig EncConfig;
    EncConfig.Algorithm = Helianthus::MessageQueue::EncryptionAlgorithm::AES_128_CBC;
    EncConfig.Key = "MySecretKey12345";
    EncConfig.IV = "MyIV1234567890123";
    EncConfig.EnableAutoEncryption = true;

    Queue->SetCompressionConfig(QueueName, CompConfig);
    Queue->SetEncryptionConfig(QueueName, EncConfig);

    const int TestIterations = 5;
    const int MessageCount = 1000;
    const size_t MessageSize = 2048; // 2KB

    std::vector<double> SendTimes, ReceiveTimes, BatchTimes, ZeroCopyTimes;

    for (int iter = 0; iter < TestIterations; ++iter)
    {
        // 准备消息
        std::vector<std::shared_ptr<Helianthus::MessageQueue::Message>> Messages;
        Messages.reserve(MessageCount);
        
        for (int i = 0; i < MessageCount; ++i)
        {
            auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
            std::string Payload = GenerateCompressibleData(MessageSize);
            Msg->Payload.Data.assign(Payload.begin(), Payload.end());
            Msg->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
            Messages.push_back(Msg);
        }

        // 测试发送
        double SendTime = MeasureTime([&]() {
            for (const auto& Msg : Messages)
            {
                Queue->SendMessage(QueueName, Msg);
            }
        });
        SendTimes.push_back(SendTime);

        // 测试接收
        double ReceiveTime = MeasureTime([&]() {
            for (int i = 0; i < MessageCount; ++i)
            {
                Helianthus::MessageQueue::MessagePtr ReceivedMsg;
                Queue->ReceiveMessage(QueueName, ReceivedMsg);
            }
        });
        ReceiveTimes.push_back(ReceiveTime);

        // 测试批处理
        double BatchTime = MeasureTime([&]() {
            uint32_t BatchId = 0;
            Queue->CreateBatchForQueue(QueueName, BatchId);
            
            for (const auto& Msg : Messages)
            {
                Queue->AddToBatch(BatchId, Msg);
            }
            
            Queue->CommitBatch(BatchId);
        });
        BatchTimes.push_back(BatchTime);

        // 测试零拷贝
        double ZeroCopyTime = MeasureTime([&]() {
            for (const auto& Msg : Messages)
            {
                Helianthus::MessageQueue::ZeroCopyBuffer ZB{};
                std::string Data(Msg->Payload.Data.begin(), Msg->Payload.Data.end());
                Queue->CreateZeroCopyBuffer(Data.c_str(), Data.size(), ZB);
                Queue->SendMessageZeroCopy(QueueName, ZB);
                Queue->ReleaseZeroCopyBuffer(ZB);
            }
        });
        ZeroCopyTimes.push_back(ZeroCopyTime);
    }

    // 计算平均值
    auto CalculateAverage = [](const std::vector<double>& Values) {
        return std::accumulate(Values.begin(), Values.end(), 0.0) / Values.size();
    };

    double AvgSendTime = CalculateAverage(SendTimes);
    double AvgReceiveTime = CalculateAverage(ReceiveTimes);
    double AvgBatchTime = CalculateAverage(BatchTimes);
    double AvgZeroCopyTime = CalculateAverage(ZeroCopyTimes);

    std::cout << "\n=== 综合性能测试结果 ===" << std::endl;
    std::cout << "测试迭代次数: " << TestIterations << std::endl;
    std::cout << "消息数量: " << MessageCount << std::endl;
    std::cout << "消息大小: " << MessageSize << " 字节" << std::endl;
    std::cout << "平均发送时间: " << std::fixed << std::setprecision(2) << AvgSendTime << " ms" << std::endl;
    std::cout << "平均接收时间: " << AvgReceiveTime << " ms" << std::endl;
    std::cout << "平均批处理时间: " << AvgBatchTime << " ms" << std::endl;
    std::cout << "平均零拷贝时间: " << AvgZeroCopyTime << " ms" << std::endl;
    std::cout << "发送吞吐量: " << CalculateThroughput(MessageCount, AvgSendTime) << " 消息/秒" << std::endl;
    std::cout << "接收吞吐量: " << CalculateThroughput(MessageCount, AvgReceiveTime) << " 消息/秒" << std::endl;
    std::cout << "批处理吞吐量: " << CalculateThroughput(MessageCount, AvgBatchTime) << " 消息/秒" << std::endl;
    std::cout << "零拷贝吞吐量: " << CalculateThroughput(MessageCount, AvgZeroCopyTime) << " 消息/秒" << std::endl;

    // 验证综合性能
    EXPECT_GT(CalculateThroughput(MessageCount, AvgSendTime), 500);
    EXPECT_GT(CalculateThroughput(MessageCount, AvgReceiveTime), 500);
    EXPECT_GT(CalculateThroughput(MessageCount, AvgBatchTime), 1000);
    EXPECT_GT(CalculateThroughput(MessageCount, AvgZeroCopyTime), 1000);
}
