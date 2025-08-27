#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace Helianthus::MessageQueue;

int main()
{
    std::cout << "=== 事务功能测试开始 ===" << std::endl;
    
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
    Config.Name = "transaction_test_queue";
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

    // 设置事务回调
    Queue->SetTransactionCommitHandler([](TransactionId Id, bool Success, const std::string& ErrorMessage) {
        std::cout << "事务提交回调: id=" << Id << ", success=" << (Success ? "true" : "false") 
                  << ", error=" << ErrorMessage << std::endl;
    });

    Queue->SetTransactionRollbackHandler([](TransactionId Id, const std::string& Reason) {
        std::cout << "事务回滚回调: id=" << Id << ", reason=" << Reason << std::endl;
    });

    Queue->SetTransactionTimeoutHandler([](TransactionId Id) {
        std::cout << "事务超时回调: id=" << Id << std::endl;
    });

    // 测试1：成功的事务
    std::cout << "=== 测试1：成功的事务 ===" << std::endl;
    
    TransactionId TxId1 = Queue->BeginTransaction("测试事务1", 10000);
    std::cout << "开始事务: id=" << TxId1 << std::endl;

    // 在事务中发送消息
    auto Message1 = std::make_shared<Helianthus::MessageQueue::Message>();
    Message1->Header.Type = MessageType::TEXT;
    Message1->Header.Priority = MessagePriority::NORMAL;
    Message1->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    Message1->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    
    std::string Payload1 = "事务测试消息1";
    Message1->Payload.Data = std::vector<char>(Payload1.begin(), Payload1.end());
    Message1->Payload.Size = Message1->Payload.Data.size();

    auto SendResult1 = Queue->SendMessageInTransaction(TxId1, Config.Name, Message1);
    if (SendResult1 == QueueResult::SUCCESS)
    {
        std::cout << "事务内发送消息成功: id=" << Message1->Header.Id << std::endl;
    }

    // 提交事务
    auto CommitResult1 = Queue->CommitTransaction(TxId1);
    if (CommitResult1 == QueueResult::SUCCESS)
    {
        std::cout << "事务提交成功: id=" << TxId1 << std::endl;
    }
    else
    {
        std::cout << "事务提交失败: id=" << TxId1 << ", error=" << static_cast<int>(CommitResult1) << std::endl;
    }

    // 测试2：回滚的事务
    std::cout << "=== 测试2：回滚的事务 ===" << std::endl;
    
    TransactionId TxId2 = Queue->BeginTransaction("测试事务2", 10000);
    std::cout << "开始事务: id=" << TxId2 << std::endl;

    // 在事务中发送消息
    auto Message2 = std::make_shared<Helianthus::MessageQueue::Message>();
    Message2->Header.Type = MessageType::TEXT;
    Message2->Header.Priority = MessagePriority::NORMAL;
    Message2->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
    Message2->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    
    std::string Payload2 = "事务测试消息2（将被回滚）";
    Message2->Payload.Data = std::vector<char>(Payload2.begin(), Payload2.end());
    Message2->Payload.Size = Message2->Payload.Data.size();

    auto SendResult2 = Queue->SendMessageInTransaction(TxId2, Config.Name, Message2);
    if (SendResult2 == QueueResult::SUCCESS)
    {
        std::cout << "事务内发送消息成功: id=" << Message2->Header.Id << std::endl;
    }

    // 回滚事务
    auto RollbackResult2 = Queue->RollbackTransaction(TxId2, "测试回滚");
    if (RollbackResult2 == QueueResult::SUCCESS)
    {
        std::cout << "事务回滚成功: id=" << TxId2 << std::endl;
    }
    else
    {
        std::cout << "事务回滚失败: id=" << TxId2 << ", error=" << static_cast<int>(RollbackResult2) << std::endl;
    }

    // 测试3：查看事务统计
    std::cout << "=== 测试3：查看事务统计 ===" << std::endl;
    
    TransactionStats Stats;
    auto StatsResult = Queue->GetTransactionStats(Stats);
    if (StatsResult == QueueResult::SUCCESS)
    {
        std::cout << "事务统计:" << std::endl;
        std::cout << "  总事务数: " << Stats.TotalTransactions << std::endl;
        std::cout << "  已提交: " << Stats.CommittedTransactions << std::endl;
        std::cout << "  已回滚: " << Stats.RolledBackTransactions << std::endl;
        std::cout << "  超时: " << Stats.TimeoutTransactions << std::endl;
        std::cout << "  失败: " << Stats.FailedTransactions << std::endl;
        std::cout << "  成功率: " << (Stats.SuccessRate * 100) << "%" << std::endl;
        std::cout << "  回滚率: " << (Stats.RollbackRate * 100) << "%" << std::endl;
        std::cout << "  平均提交时间: " << Stats.AverageCommitTimeMs << "ms" << std::endl;
        std::cout << "  平均回滚时间: " << Stats.AverageRollbackTimeMs << "ms" << std::endl;
    }

    // 测试4：查看事务状态
    std::cout << "=== 测试4：查看事务状态 ===" << std::endl;
    
    TransactionStatus Status1, Status2;
    Queue->GetTransactionStatus(TxId1, Status1);
    Queue->GetTransactionStatus(TxId2, Status2);
    
    std::cout << "事务1状态: " << static_cast<int>(Status1) << std::endl;
    std::cout << "事务2状态: " << static_cast<int>(Status2) << std::endl;

    // 等待一段时间观察超时
    std::cout << "等待5秒观察事务超时..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 清理
    std::cout << "=== 事务功能测试完成 ===" << std::endl;

    return 0;
}
