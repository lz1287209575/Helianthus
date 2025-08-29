#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

#include <iostream>
#include <string>

using namespace Helianthus::MessageQueue;

int main()
{
    std::cout << "=== 压缩和加密功能测试开始 ===" << std::endl;
    
    // 创建消息队列实例
    auto Queue = std::make_unique<MessageQueue>();
    std::cout << "创建消息队列实例" << std::endl;

    // 初始化消息队列
    std::cout << "开始初始化消息队列..." << std::endl;
    auto InitResult = Queue->Initialize();
    if (InitResult != QueueResult::SUCCESS)
    {
        std::cout << "消息队列初始化失败: " << static_cast<int>(InitResult) << std::endl;
        return 1;
    }
    std::cout << "消息队列初始化成功" << std::endl;

    // 创建测试队列
    QueueConfig Config;
    Config.Name = "compression_encryption_test_queue";
    Config.MaxSize = 1000;
    Config.MaxSizeBytes = 1024 * 1024 * 100; // 100MB
    Config.MessageTtlMs = 30000; // 30秒
    Config.EnableDeadLetter = true;
    Config.EnablePriority = false;
    Config.EnableBatching = false;

    auto CreateResult = Queue->CreateQueue(Config);
    if (CreateResult != QueueResult::SUCCESS)
    {
        std::cout << "创建队列失败: " << static_cast<int>(CreateResult) << std::endl;
        return 1;
    }
    std::cout << "创建队列成功: " << Config.Name << std::endl;
    // 放大队列容量以便批量基准
    Config.MaxSize = 100000;
    auto UpdateRes = Queue->UpdateQueueConfig(Config.Name, Config);
    if (UpdateRes != QueueResult::SUCCESS)
    {
        std::cout << "更新队列配置失败: code=" << static_cast<int>(UpdateRes) << std::endl;
    }

    // 测试1：设置压缩配置
    std::cout << "=== 测试1：设置压缩配置 ===" << std::endl;
    
    CompressionConfig CompConfig;
    CompConfig.Algorithm = CompressionAlgorithm::GZIP;
    CompConfig.Level = 6;
    CompConfig.MinSize = 1024;
    CompConfig.EnableAutoCompression = true;
    
    auto SetCompResult = Queue->SetCompressionConfig(Config.Name, CompConfig);
    if (SetCompResult == QueueResult::SUCCESS)
    {
        std::cout << "设置压缩配置成功" << std::endl;
    }
    else
    {
        std::cout << "设置压缩配置失败: " << static_cast<int>(SetCompResult) << std::endl;
    }

    // 测试2：设置加密配置
    std::cout << "=== 测试2：设置加密配置 ===" << std::endl;
    
    EncryptionConfig EncConfig;
    EncConfig.Algorithm = EncryptionAlgorithm::AES_256_GCM;
    EncConfig.Key = "0123456789abcdef0123456789abcdef"; // 32字节密钥
    EncConfig.IV = "0123456789abcdef"; // 16字节IV
    EncConfig.EnableAutoEncryption = true;
    
    auto SetEncResult = Queue->SetEncryptionConfig(Config.Name, EncConfig);
    if (SetEncResult == QueueResult::SUCCESS)
    {
        std::cout << "设置加密配置成功" << std::endl;
    }
    else
    {
        std::cout << "设置加密配置失败: " << static_cast<int>(SetEncResult) << std::endl;
    }

    // 测试3：查询配置
    std::cout << "=== 测试3：查询配置 ===" << std::endl;
    
    CompressionConfig RetrievedCompConfig;
    auto GetCompResult = Queue->GetCompressionConfig(Config.Name, RetrievedCompConfig);
    if (GetCompResult == QueueResult::SUCCESS)
    {
        std::cout << "查询压缩配置成功: algorithm=" << static_cast<int>(RetrievedCompConfig.Algorithm) 
                  << ", level=" << RetrievedCompConfig.Level << ", min_size=" << RetrievedCompConfig.MinSize << std::endl;
    }
    
    EncryptionConfig RetrievedEncConfig;
    auto GetEncResult = Queue->GetEncryptionConfig(Config.Name, RetrievedEncConfig);
    if (GetEncResult == QueueResult::SUCCESS)
    {
        std::cout << "查询加密配置成功: algorithm=" << static_cast<int>(RetrievedEncConfig.Algorithm) 
                  << ", auto_encrypt=" << RetrievedEncConfig.EnableAutoEncryption 
                  << ", key_size=" << RetrievedEncConfig.Key.size()
                  << ", iv_size=" << RetrievedEncConfig.IV.size() << std::endl;
    }

    // 测试4：手动压缩和加密消息
    std::cout << "=== 测试4：手动压缩和加密消息 ===" << std::endl;
    
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = MessageType::TEXT;
    Message->Header.Priority = MessagePriority::NORMAL;
    Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    Message->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    
    // 创建一个较大的消息内容用于测试压缩
    std::string LargePayload;
    for (int i = 0; i < 1000; ++i)
    {
        LargePayload += "这是一个重复的文本内容，用于测试压缩算法的效果。";
    }
    
    Message->Payload.Data = std::vector<char>(LargePayload.begin(), LargePayload.end());
    Message->Payload.Size = Message->Payload.Data.size();
    
    std::cout << "原始消息大小: " << Message->Payload.Data.size() << " 字节" << std::endl;
    
    // 压缩消息
    auto CompressResult = Queue->CompressMessage(Message, CompressionAlgorithm::GZIP);
    if (CompressResult == QueueResult::SUCCESS)
    {
        std::cout << "消息压缩成功，压缩后大小: " << Message->Payload.Data.size() << " 字节" << std::endl;
    }
    else
    {
        std::cout << "消息压缩失败: " << static_cast<int>(CompressResult) << std::endl;
    }
    
    // 加密消息
    auto EncryptResult = Queue->EncryptMessage(Message, EncryptionAlgorithm::AES_256_GCM, EncConfig);
    if (EncryptResult == QueueResult::SUCCESS)
    {
        std::cout << "消息加密成功，加密后大小: " << Message->Payload.Data.size() << " 字节" << std::endl;
    }
    else
    {
        std::cout << "消息加密失败: " << static_cast<int>(EncryptResult) << std::endl;
    }

    // 测试5：发送压缩和加密的消息
    std::cout << "=== 测试5：发送压缩和加密的消息 ===" << std::endl;
    
    auto SendResult = Queue->SendMessage(Config.Name, Message);
    if (SendResult == QueueResult::SUCCESS)
    {
        std::cout << "发送压缩和加密消息成功: id=" << Message->Header.Id << std::endl;
    }
    else
    {
        std::cout << "发送消息失败: " << static_cast<int>(SendResult) << std::endl;
    }

    // 测试6：查询统计信息
    std::cout << "=== 测试6：查询统计信息 ===" << std::endl;
    
    CompressionStats CompStats;
    auto GetCompStatsResult = Queue->GetCompressionStats(Config.Name, CompStats);
    if (GetCompStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "压缩统计:" << std::endl;
        std::cout << "  总消息数: " << CompStats.TotalMessages << std::endl;
        std::cout << "  已压缩消息数: " << CompStats.CompressedMessages << std::endl;
        std::cout << "  原始字节数: " << CompStats.OriginalBytes << std::endl;
        std::cout << "  压缩后字节数: " << CompStats.CompressedBytes << std::endl;
        std::cout << "  压缩比: " << (CompStats.CompressionRatio * 100) << "%" << std::endl;
        std::cout << "  平均压缩时间: " << CompStats.AverageCompressionTimeMs << "ms" << std::endl;
    }
    
    EncryptionStats EncStats;
    auto GetEncStatsResult = Queue->GetEncryptionStats(Config.Name, EncStats);
    if (GetEncStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "加密统计:" << std::endl;
        std::cout << "  总消息数: " << EncStats.TotalMessages << std::endl;
        std::cout << "  已加密消息数: " << EncStats.EncryptedMessages << std::endl;
        std::cout << "  平均加密时间: " << EncStats.AverageEncryptionTimeMs << "ms" << std::endl;
        std::cout << "  平均解密时间: " << EncStats.AverageDecryptionTimeMs << "ms" << std::endl;
    }

    // 测试7：发送未手动压缩/加密的消息，验证自动管线与统计
    std::cout << "=== 测试7：自动压缩/加密与统计验证 ===" << std::endl;
    auto AutoMsg = std::make_shared<Helianthus::MessageQueue::Message>();
    AutoMsg->Header.Type = MessageType::TEXT;
    AutoMsg->Header.Priority = MessagePriority::NORMAL;
    AutoMsg->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    AutoMsg->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    std::string AutoPayload(8192, 'A');
    AutoMsg->Payload.Data = std::vector<char>(AutoPayload.begin(), AutoPayload.end());
    AutoMsg->Payload.Size = AutoMsg->Payload.Data.size();
    // 检查发送前的消息属性
    std::cout << "发送前消息属性数量: " << AutoMsg->Header.Properties.size() << std::endl;
    for (const auto& [Key, Value] : AutoMsg->Header.Properties)
    {
        std::cout << "  " << Key << " = " << Value << std::endl;
    }
    
    auto AutoSend = Queue->SendMessage(Config.Name, AutoMsg);
    if (AutoSend == QueueResult::SUCCESS)
    {
        std::cout << "自动消息发送成功: id=" << AutoMsg->Header.Id << std::endl;
        // 检查发送后的消息属性
        std::cout << "发送后消息属性数量: " << AutoMsg->Header.Properties.size() << std::endl;
        for (const auto& [Key, Value] : AutoMsg->Header.Properties)
        {
            std::cout << "  " << Key << " = " << Value << std::endl;
        }
    }
    else
    {
        std::cout << "自动消息发送失败: code=" << static_cast<int>(AutoSend) << std::endl;
    }

    // 再次查询统计
    GetCompStatsResult = Queue->GetCompressionStats(Config.Name, CompStats);
    GetEncStatsResult = Queue->GetEncryptionStats(Config.Name, EncStats);
    if (GetCompStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "自动后压缩统计:" << std::endl;
        std::cout << "  总消息数: " << CompStats.TotalMessages << std::endl;
        std::cout << "  已压缩消息数: " << CompStats.CompressedMessages << std::endl;
        std::cout << "  原始字节数: " << CompStats.OriginalBytes << std::endl;
        std::cout << "  压缩后字节数: " << CompStats.CompressedBytes << std::endl;
        std::cout << "  压缩比: " << (CompStats.CompressionRatio * 100) << "%" << std::endl;
        std::cout << "  平均压缩时间: " << CompStats.AverageCompressionTimeMs << "ms" << std::endl;
    }
    if (GetEncStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "自动后加密统计:" << std::endl;
        std::cout << "  总消息数: " << EncStats.TotalMessages << std::endl;
        std::cout << "  已加密消息数: " << EncStats.EncryptedMessages << std::endl;
        std::cout << "  平均加密时间: " << EncStats.AverageEncryptionTimeMs << "ms" << std::endl;
        std::cout << "  平均解密时间: " << EncStats.AverageDecryptionTimeMs << "ms" << std::endl;
    }

    // 回环验证：接收消息，自动解密/解压后验证内容
    std::cout << "=== 测试8：回环验证（自动解密/解压） ===" << std::endl;
    Helianthus::MessageQueue::MessagePtr RecvMsg;
    bool Verified = false;
    std::string ExpectA(8192, 'A');
    std::string ExpectLarge(LargePayload.begin(), LargePayload.end());
    for (int tries = 0; tries < 4 && !Verified; ++tries)
    {
        auto RecvRes = Queue->ReceiveMessage(Config.Name, RecvMsg, 1000);
        if (RecvRes == QueueResult::SUCCESS && RecvMsg)
        {
            // 详细检查消息属性
            std::cout << "接收消息 ID=" << RecvMsg->Header.Id << " size=" << RecvMsg->Payload.Data.size() << std::endl;
            std::cout << "  属性数量: " << RecvMsg->Header.Properties.size() << std::endl;
            for (const auto& [Key, Value] : RecvMsg->Header.Properties)
            {
                std::cout << "    " << Key << " = " << Value << std::endl;
            }
            
            std::string Actual(RecvMsg->Payload.Data.begin(), RecvMsg->Payload.Data.end());
            if (Actual == ExpectA || Actual == ExpectLarge)
            {
                std::cout << "回环验证成功：内容一致 (" << Actual.size() << " bytes)" << std::endl;
                Verified = true;
                break;
            }
            else
            {
                std::cout << "回环验证未命中：size=" << Actual.size() << std::endl;
                // 显示前100个字符用于调试
                std::string Preview = Actual.substr(0, std::min(100ul, Actual.size()));
                std::cout << "  内容预览: " << Preview << std::endl;
                // 检查是否包含压缩/加密的标记
                bool HasCompressed = RecvMsg->Header.Properties.count("Compressed");
                bool HasEncrypted = RecvMsg->Header.Properties.count("Encrypted");
                std::cout << "  压缩标记: " << (HasCompressed ? "有" : "无") << std::endl;
                std::cout << "  加密标记: " << (HasEncrypted ? "有" : "无") << std::endl;
            }
        }
        else
        {
            std::cout << "接收失败: code=" << static_cast<int>(RecvRes) << std::endl;
        }
    }
    if (!Verified)
    {
        std::cout << "回环验证失败：未匹配到期望内容" << std::endl;
    }

    // 批量基准：发送 N 条，统计吞吐与均值耗时
    std::cout << "=== 测试9：批量基准（吞吐/均值耗时） ===" << std::endl;
    const int BatchN = 5000;
    std::vector<char> BenchPayload(2048, 'B');
    auto Start = std::chrono::high_resolution_clock::now();
    int Sent = 0;
    for (int i = 0; i < BatchN; ++i)
    {
        auto Msg = std::make_shared<Helianthus::MessageQueue::Message>();
        Msg->Header.Type = MessageType::TEXT;
        Msg->Header.Priority = MessagePriority::NORMAL;
        Msg->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        Msg->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
        Msg->Payload.Data = BenchPayload;
        Msg->Payload.Size = BenchPayload.size();
        auto R = Queue->SendMessage(Config.Name, Msg);
        if (R != QueueResult::SUCCESS)
        {
            std::cout << "发送失败: i=" << i << " code=" << static_cast<int>(R) << std::endl;
            break;
        }
        ++Sent;
    }
    auto End = std::chrono::high_resolution_clock::now();
    double Ms = std::chrono::duration<double, std::milli>(End - Start).count();
    double Throughput = (Sent / (Ms / 1000.0));
    std::cout << "批量发送: N=" << Sent << ", 总耗时(ms)=" << Ms
              << ", 吞吐(msg/s)=" << Throughput
              << ", 平均耗时(us)=" << (Ms * 1000.0 / std::max(1, Sent)) << std::endl;

    // 清理
    std::cout << "=== 压缩和加密功能测试完成 ===" << std::endl;

    return 0;
}
