#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessagePersistence.h"
#include "Shared/Common/Logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace Helianthus::MessageQueue;
using namespace Helianthus::Common;

int main()
{
    // 初始化日志系统
    Logger::Initialize();
    
    // 创建消息队列
    auto Queue = std::make_unique<MessageQueue>();
    
    // 初始化队列
    QueueResult Result = Queue->Initialize();
    if (Result != QueueResult::SUCCESS)
    {
        std::cerr << "队列初始化失败: " << static_cast<int>(Result) << std::endl;
        return 1;
    }
    
    std::cout << "=== 持久化耗时指标演示 ===" << std::endl;
    
    // 创建测试队列
    QueueConfig QueueConfig;
    QueueConfig.Name = "test_queue";
    QueueConfig.Type = QueueType::STANDARD;
    QueueConfig.MaxSize = 1000;
    QueueConfig.MaxSizeBytes = 10 * 1024 * 1024;  // 10MB
    QueueConfig.Persistence = PersistenceMode::DISK_PERSISTENT;
    
    Result = Queue->CreateQueue(QueueConfig);
    if (Result != QueueResult::SUCCESS)
    {
        std::cerr << "创建队列失败: " << static_cast<int>(Result) << std::endl;
        return 1;
    }
    
    // 发送一些消息来产生持久化操作
    std::cout << "\n1. 发送消息产生写入操作..." << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        std::string Payload = "测试消息 #" + std::to_string(i + 1) + " - " + 
                             std::string(100, 'A' + (i % 26)); // 100字节的测试数据
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>(MessageType::TEXT, Payload);
        Message->Header.Id = static_cast<MessageId>(i + 1);
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        Result = Queue->SendMessage("test_queue", Message);
        if (Result != QueueResult::SUCCESS)
        {
            std::cerr << "发送消息失败: " << static_cast<int>(Result) << std::endl;
        }
        else
        {
            std::cout << "发送消息 " << (i + 1) << std::endl;
        }
        
        // 短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 读取一些消息来产生读取操作
    std::cout << "\n2. 接收消息产生读取操作..." << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        MessagePtr ReceivedMessage;
        Result = Queue->ReceiveMessage("test_queue", ReceivedMessage, 1000);
        if (Result == QueueResult::SUCCESS)
        {
            std::cout << "接收消息 ID: " << static_cast<uint64_t>(ReceivedMessage->Header.Id) << std::endl;
            
            // 确认消息
            Queue->AcknowledgeMessage("test_queue", ReceivedMessage->Header.Id);
        }
        else if (Result == QueueResult::TIMEOUT)
        {
            std::cout << "接收超时" << std::endl;
        }
        else
        {
            std::cout << "接收失败: " << static_cast<int>(Result) << std::endl;
        }
        
        // 短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 获取持久化统计信息
    std::cout << "\n3. 持久化耗时统计信息:" << std::endl;
    
    auto PersistenceStats = Queue->GetPersistenceStats();
    
    std::cout << "写入操作统计:" << std::endl;
    std::cout << "  总写入次数: " << PersistenceStats.TotalWriteCount << std::endl;
    std::cout << "  总写入时间: " << PersistenceStats.TotalWriteTimeMs << " ms" << std::endl;
    std::cout << "  平均写入时间: " << PersistenceStats.GetAverageWriteTimeMs() << " ms" << std::endl;
    std::cout << "  最大写入时间: " << PersistenceStats.MaxWriteTimeMs << " ms" << std::endl;
    std::cout << "  最小写入时间: " << PersistenceStats.MinWriteTimeMs << " ms" << std::endl;
    
    std::cout << "\n读取操作统计:" << std::endl;
    std::cout << "  总读取次数: " << PersistenceStats.TotalReadCount << std::endl;
    std::cout << "  总读取时间: " << PersistenceStats.TotalReadTimeMs << " ms" << std::endl;
    std::cout << "  平均读取时间: " << PersistenceStats.GetAverageReadTimeMs() << " ms" << std::endl;
    std::cout << "  最大读取时间: " << PersistenceStats.MaxReadTimeMs << " ms" << std::endl;
    std::cout << "  最小读取时间: " << PersistenceStats.MinReadTimeMs << " ms" << std::endl;
    
    // 重置统计信息
    std::cout << "\n4. 重置统计信息..." << std::endl;
    Queue->ResetPersistenceStats();
    
    // 再次获取统计信息（应该都是0）
    PersistenceStats = Queue->GetPersistenceStats();
    std::cout << "重置后的统计信息:" << std::endl;
    std::cout << "  总写入次数: " << PersistenceStats.TotalWriteCount << std::endl;
    std::cout << "  总读取次数: " << PersistenceStats.TotalReadCount << std::endl;
    
    // 获取队列指标
    std::cout << "\n5. 队列指标信息:" << std::endl;
    QueueMetrics Metrics;
    if (Queue->GetQueueMetrics("test_queue", Metrics) == QueueResult::SUCCESS)
    {
        std::cout << "队列长度: " << Metrics.PendingMessages << std::endl;
        std::cout << "累计发送: " << Metrics.TotalMessages << std::endl;
        std::cout << "累计接收: " << Metrics.ProcessedMessages << std::endl;
        std::cout << "入队速率: " << Metrics.EnqueueRate << " msg/s" << std::endl;
        std::cout << "出队速率: " << Metrics.DequeueRate << " msg/s" << std::endl;
        std::cout << "处理延迟 P50: " << Metrics.P50LatencyMs << " ms" << std::endl;
        std::cout << "处理延迟 P95: " << Metrics.P95LatencyMs << " ms" << std::endl;
    }
    
    // 清理
    std::cout << "\n6. 清理资源..." << std::endl;
    Queue->DeleteQueue("test_queue");
    Queue->Shutdown();
    Logger::Shutdown();
    
    std::cout << "\n=== 演示完成 ===" << std::endl;
    return 0;
}
