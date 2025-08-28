#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/Logger.h"
#include <iostream>

using namespace Helianthus::MessageQueue;
using namespace Helianthus::Common;

int main()
{
    Logger::Initialize();
    std::cout << "=== Helianthus 事务支持演示 ===" << std::endl;
    
    auto Queue = std::make_unique<MessageQueue>();
    QueueResult Result = Queue->Initialize();
    if (Result != QueueResult::SUCCESS)
    {
        std::cerr << "队列初始化失败: " << static_cast<int>(Result) << std::endl;
        return 1;
    }
    
    // 创建测试队列
    QueueConfig QueueConfig;
    QueueConfig.Name = "transaction_test_queue";
    QueueConfig.Type = QueueType::STANDARD;
    QueueConfig.MaxSize = 1000;
    QueueConfig.Persistence = PersistenceMode::DISK_PERSISTENT;
    
    Result = Queue->CreateQueue(QueueConfig);
    if (Result != QueueResult::SUCCESS)
    {
        std::cerr << "创建队列失败: " << static_cast<int>(Result) << std::endl;
        return 1;
    }
    
    // 演示事务
    std::cout << "开始事务演示..." << std::endl;
    
    TransactionId TxId = Queue->BeginTransaction("测试事务", 30000);
    std::cout << "事务ID: " << TxId << std::endl;
    
    // 在事务中发送消息
    auto TestMessage = std::make_shared<Message>(MessageType::TEXT, "事务测试消息");
    Result = Queue->SendMessageInTransaction(TxId, QueueConfig.Name, TestMessage);
    
    if (Result == QueueResult::SUCCESS)
    {
        std::cout << "消息发送成功，提交事务..." << std::endl;
        Result = Queue->CommitTransaction(TxId);
        
        if (Result == QueueResult::SUCCESS)
        {
            std::cout << "事务提交成功！" << std::endl;
        }
        else
        {
            std::cout << "事务提交失败: " << static_cast<int>(Result) << std::endl;
        }
    }
    else
    {
        std::cout << "消息发送失败，回滚事务..." << std::endl;
        Queue->RollbackTransaction(TxId, "发送失败");
    }
    
    // 显示统计
    TransactionStats Stats;
    if (Queue->GetTransactionStats(Stats) == QueueResult::SUCCESS)
    {
        std::cout << "事务统计: 总数=" << Stats.TotalTransactions 
                  << ", 成功=" << Stats.CommittedTransactions 
                  << ", 回滚=" << Stats.RolledBackTransactions << std::endl;
    }
    
    std::cout << "演示完成！" << std::endl;
    return 0;
}
