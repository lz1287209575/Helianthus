# API 参考

## 📚 概述

Helianthus 提供了完整的 C++ API，包括消息队列、事务处理、监控等功能。

## 🏗️ 核心组件

### IMessageQueue

消息队列接口，定义了所有队列操作。

#### 主要方法

```cpp
class IMessageQueue {
public:
    // 初始化
    virtual QueueResult Initialize(const MessageQueueConfig& Config) = 0;
    
    // 队列管理
    virtual QueueResult CreateQueue(const QueueConfig& Config) = 0;
    virtual QueueResult DeleteQueue(const std::string& QueueName) = 0;
    
    // 消息操作
    virtual QueueResult SendMessage(const std::string& QueueName, const MessagePtr& Message) = 0;
    virtual QueueResult ReceiveMessage(const std::string& QueueName, MessagePtr& Message) = 0;
    
    // 事务操作
    virtual TransactionId BeginTransaction(const std::string& Description = "", uint32_t TimeoutMs = 30000) = 0;
    virtual QueueResult CommitTransaction(TransactionId Id) = 0;
    virtual QueueResult RollbackTransaction(TransactionId Id, const std::string& Reason = "") = 0;
    
    // 分布式事务
    virtual QueueResult BeginDistributedTransaction(const std::string& CoordinatorId, const std::string& Description = "", uint32_t TimeoutMs = 30000) = 0;
    virtual QueueResult PrepareTransaction(TransactionId Id) = 0;
    virtual QueueResult CommitDistributedTransaction(TransactionId Id) = 0;
    virtual QueueResult RollbackDistributedTransaction(TransactionId Id, const std::string& Reason = "") = 0;
    
    // 统计信息
    virtual void GetPerformanceStats(PerformanceStats& Stats) = 0;
    virtual void GetTransactionStats(TransactionStats& Stats) = 0;
};
```

### MessageQueue

消息队列的具体实现。

```cpp
class MessageQueue : public IMessageQueue {
public:
    MessageQueue();
    ~MessageQueue() override;
    
    // 实现 IMessageQueue 接口
    QueueResult Initialize(const MessageQueueConfig& Config) override;
    // ... 其他方法实现
};
```

## 📦 数据结构

### QueueConfig

队列配置结构：

```cpp
struct QueueConfig {
    std::string Name;                    // 队列名称
    PersistenceMode Persistence;        // 持久化模式
    uint32_t MaxSize;                    // 最大队列大小
    uint32_t TimeoutMs;                  // 超时时间
    CompressionConfig Compression;       // 压缩配置
    EncryptionConfig Encryption;        // 加密配置
};
```

### Message

消息结构：

```cpp
struct Message {
    MessageHeader Header;               // 消息头
    MessagePayload Payload;             // 消息体
    std::map<std::string, std::string> Metadata;  // 元数据
};
```

### MessageHeader

消息头结构：

```cpp
struct MessageHeader {
    MessageType Type;                   // 消息类型
    uint64_t Timestamp;                 // 时间戳
    uint32_t Size;                      // 消息大小
    std::string Id;                     // 消息ID
    uint32_t Priority;                  // 优先级
};
```

### MessagePayload

消息体结构：

```cpp
struct MessagePayload {
    std::vector<uint8_t> Data;          // 原始数据
    CompressionAlgorithm Compression;   // 压缩算法
    EncryptionAlgorithm Encryption;     // 加密算法
};
```

## 🔧 配置选项

### PersistenceMode

持久化模式枚举：

```cpp
enum class PersistenceMode {
    MEMORY_ONLY,        // 仅内存
    FILE_BASED,         // 文件持久化
    DATABASE_BASED      // 数据库持久化
};
```

### CompressionConfig

压缩配置：

```cpp
struct CompressionConfig {
    CompressionAlgorithm Algorithm;     // 压缩算法
    uint8_t Level;                      // 压缩级别 (1-9)
    uint32_t MinSize;                   // 最小压缩大小
    bool EnableAutoCompression;         // 自动压缩
};
```

### EncryptionConfig

加密配置：

```cpp
struct EncryptionConfig {
    EncryptionAlgorithm Algorithm;      // 加密算法
    std::string Key;                   // 加密密钥
    std::vector<uint8_t> IV;            // 初始化向量
    bool EnableAutoEncryption;          // 自动加密
};
```

## 📊 监控 API

### EnhancedPrometheusExporter

Prometheus 指标导出器：

```cpp
class EnhancedPrometheusExporter {
public:
    void Start(const std::string& Host, uint16_t Port);
    void Stop();
    
    // 指标更新
    void UpdateBatchMetrics(const BatchStats& Stats);
    void UpdateZeroCopyMetrics(const ZeroCopyStats& Stats);
    void UpdateTransactionMetrics(const TransactionStats& Stats);
    
    // 获取指标
    std::string GetMetrics() const;
};
```

### 性能统计

```cpp
struct PerformanceStats {
    // 批处理统计
    uint64_t TotalBatches;
    uint64_t TotalMessages;
    double AverageBatchTimeMs;
    double P50BatchTimeMs;
    double P95BatchTimeMs;
    double P99BatchTimeMs;
    
    // 零拷贝统计
    uint64_t TotalZeroCopyOperations;
    double AverageZeroCopyTimeMs;
    double P50ZeroCopyTimeMs;
    double P95ZeroCopyTimeMs;
    double P99ZeroCopyTimeMs;
};
```

### 事务统计

```cpp
struct TransactionStats {
    uint64_t TotalTransactions;
    uint64_t CommittedTransactions;
    uint64_t RolledBackTransactions;
    uint64_t TimeoutTransactions;
    uint64_t FailedTransactions;
    
    double SuccessRate;
    double RollbackRate;
    double TimeoutRate;
    double FailureRate;
    
    double AverageCommitTimeMs;
    double P50CommitTimeMs;
    double P95CommitTimeMs;
    double P99CommitTimeMs;
};
```

## 🔄 事务 API

### 基本事务

```cpp
// 开始事务
TransactionId TxId = Queue->BeginTransaction("description", 30000);

// 在事务中操作
Queue->SendMessageInTransaction(TxId, "queue_name", message);
Queue->ReceiveMessageInTransaction(TxId, "queue_name", message);

// 提交或回滚
Queue->CommitTransaction(TxId);
Queue->RollbackTransaction(TxId, "reason");
```

### 分布式事务

```cpp
// 开始分布式事务
Queue->BeginDistributedTransaction("coordinator_id", "description", 30000);

// 两阶段提交
Queue->PrepareTransaction(TxId);
Queue->CommitDistributedTransaction(TxId);
```

## 🎯 使用示例

### 基本消息队列

```cpp
#include "Shared/MessageQueue/MessageQueue.h"

int main() {
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    // 创建队列
    QueueConfig Config;
    Config.Name = "my_queue";
    Config.Persistence = PersistenceMode::MEMORY_ONLY;
    Config.MaxSize = 10000;
    Queue->CreateQueue(Config);
    
    // 发送消息
    auto Message = std::make_shared<Message>();
    Message->Header.Type = MessageType::TEXT;
    Message->Payload.Data.assign("Hello", "Hello" + 5);
    Queue->SendMessage(Config.Name, Message);
    
    // 接收消息
    MessagePtr ReceivedMessage;
    Queue->ReceiveMessage(Config.Name, ReceivedMessage);
    
    return 0;
}
```

### 事务处理

```cpp
// 开始事务
TransactionId TxId = Queue->BeginTransaction("my_tx", 30000);

// 事务操作
auto Message = std::make_shared<Message>();
Message->Header.Type = MessageType::TEXT;
Message->Payload.Data.assign("tx_message", "tx_message" + 10);
Queue->SendMessageInTransaction(TxId, "my_queue", Message);

// 提交事务
Queue->CommitTransaction(TxId);
```

### 监控集成

```cpp
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"

int main() {
    auto Exporter = std::make_unique<EnhancedPrometheusExporter>();
    Exporter->Start("0.0.0.0", 9090);
    
    // 获取性能统计
    PerformanceStats Stats;
    Queue->GetPerformanceStats(Stats);
    
    // 更新指标
    Exporter->UpdateBatchMetrics(Stats);
    
    return 0;
}
```

## ⚠️ 错误处理

### QueueResult

操作结果枚举：

```cpp
enum class QueueResult {
    SUCCESS,                    // 成功
    QUEUE_NOT_FOUND,           // 队列不存在
    QUEUE_FULL,                // 队列已满
    QUEUE_EMPTY,               // 队列为空
    TRANSACTION_NOT_FOUND,     // 事务不存在
    INVALID_STATE,             // 无效状态
    TIMEOUT,                   // 超时
    FAILED                     // 失败
};
```

### 错误处理示例

```cpp
QueueResult Result = Queue->SendMessage("queue_name", message);
if (Result != QueueResult::SUCCESS) {
    switch (Result) {
        case QueueResult::QUEUE_NOT_FOUND:
            std::cerr << "队列不存在" << std::endl;
            break;
        case QueueResult::QUEUE_FULL:
            std::cerr << "队列已满" << std::endl;
            break;
        default:
            std::cerr << "操作失败" << std::endl;
    }
}
```

## 📈 性能考虑

### 最佳实践

1. **批量操作**: 使用批处理提高吞吐量
2. **零拷贝**: 利用零拷贝减少内存拷贝
3. **连接池**: 复用连接减少开销
4. **异步操作**: 使用异步 API 提高并发性

### 性能调优

```cpp
// 优化队列配置
QueueConfig Config;
Config.MaxSize = 50000;           // 增加队列大小
Config.TimeoutMs = 1000;          // 减少超时时间

// 启用压缩
Config.Compression.EnableAutoCompression = true;
Config.Compression.Algorithm = CompressionAlgorithm::GZIP;
Config.Compression.Level = 6;

// 启用加密
Config.Encryption.EnableAutoEncryption = true;
Config.Encryption.Algorithm = EncryptionAlgorithm::AES_256_GCM;
```

## 🔗 相关链接

- [快速开始](Getting-Started) - 入门指南
- [配置说明](Configuration) - 详细配置
- [示例代码](Examples) - 代码示例
- [性能优化](Performance) - 性能调优

---

**注意**: API 可能会在版本更新中发生变化，请查看 [更新日志](Changelog) 了解最新变化。
