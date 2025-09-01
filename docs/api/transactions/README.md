# 分布式事务 API

## 概述

Helianthus 支持完整的分布式事务功能，基于两阶段提交协议。

## 基本用法

### 开始事务

```cpp
TransactionId TxId = Queue->BeginTransaction("my_transaction", 30000);
```

### 在事务中操作

```cpp
auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
Message->Payload.Data.assign("transaction message", "transaction message" + 20);

Queue->SendMessageInTransaction(TxId, "my_queue", Message);
```

### 提交事务

```cpp
Queue->CommitTransaction(TxId);
```

### 回滚事务

```cpp
Queue->RollbackTransaction(TxId, "rollback reason");
```

## 分布式事务

### 开始分布式事务

```cpp
Queue->BeginDistributedTransaction("coordinator_001", "distributed_tx", 30000);
```

### 两阶段提交

```cpp
// 准备阶段
Queue->PrepareTransaction(TxId);

// 提交阶段
Queue->CommitDistributedTransaction(TxId);
```

## 事务统计

```cpp
TransactionStats Stats;
Queue->GetTransactionStats(Stats);

std::cout << "Total: " << Stats.TotalTransactions << std::endl;
std::cout << "Committed: " << Stats.CommittedTransactions << std::endl;
std::cout << "Success Rate: " << Stats.SuccessRate << "%" << std::endl;
```
