# 快速开始指南

## 5分钟上手

### 1. 基本消息队列

```cpp
#include "Shared/MessageQueue/MessageQueue.h"

int main() {
    // 创建队列
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    // 创建队列
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "test_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);
    
    // 发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    Message->Payload.Data.assign("Hello Helianthus!", "Hello Helianthus!" + 17);
    Queue->SendMessage(Config.Name, Message);
    
    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Queue->ReceiveMessage(Config.Name, ReceivedMessage);
    
    std::string Payload(ReceivedMessage->Payload.Data.begin(), 
                       ReceivedMessage->Payload.Data.end());
    std::cout << "Received: " << Payload << std::endl;
    
    return 0;
}
```

### 2. 事务处理

```cpp
// 开始事务
TransactionId TxId = Queue->BeginTransaction("test_transaction", 30000);

// 在事务中发送消息
auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
Message->Payload.Data.assign("transaction message", "transaction message" + 20);
Queue->SendMessageInTransaction(TxId, "test_queue", Message);

// 提交事务
Queue->CommitTransaction(TxId);
```

### 3. 性能监控

```cpp
// 启动监控
auto Exporter = std::make_unique<Helianthus::Monitoring::EnhancedPrometheusExporter>();
Exporter->Start("0.0.0.0", 9090);

// 访问指标
// http://localhost:9090/metrics
```

## 下一步

- [完整 API 文档](../api/)
- [配置指南](configuration.md)
- [部署指南](deployment.md)
- [性能优化](performance.md)
