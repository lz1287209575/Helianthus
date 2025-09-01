#include "Shared/MessageQueue/MessageQueue.h"
#include <iostream>

int main() {
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "transaction_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);
    
    // 成功的事务
    std::cout << "=== 成功事务示例 ===" << std::endl;
    Helianthus::MessageQueue::TransactionId TxId = Queue->BeginTransaction("success_tx", 30000);
    
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    Message->Payload.Data.assign("committed message", "committed message" + 17);
    Queue->SendMessageInTransaction(TxId, Config.Name, Message);
    
    Queue->CommitTransaction(TxId);
    std::cout << "事务提交成功" << std::endl;
    
    // 回滚的事务
    std::cout << "\n=== 回滚事务示例 ===" << std::endl;
    TxId = Queue->BeginTransaction("rollback_tx", 30000);
    
    Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    Message->Payload.Data.assign("rolled back message", "rolled back message" + 20);
    Queue->SendMessageInTransaction(TxId, Config.Name, Message);
    
    Queue->RollbackTransaction(TxId, "测试回滚");
    std::cout << "事务回滚成功" << std::endl;
    
    // 验证结果
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Queue->ReceiveMessage(Config.Name, ReceivedMessage);
    
    std::string Payload(ReceivedMessage->Payload.Data.begin(), 
                       ReceivedMessage->Payload.Data.end());
    std::cout << "队列中的消息: " << Payload << std::endl;
    
    return 0;
}
