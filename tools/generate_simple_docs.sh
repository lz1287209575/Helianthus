#!/bin/bash

# Helianthus 简化文档生成脚本（不依赖 Doxygen）

set -e

echo "🚀 开始生成 Helianthus 文档..."

# 创建文档目录结构
create_directories() {
    echo "📁 创建文档目录..."
    mkdir -p docs/{api,guides,examples,reports}
    mkdir -p docs/api/{message-queue,monitoring,transactions}
}

# 生成 API 文档
generate_api_docs() {
    echo "📚 生成 API 文档..."
    
    # 消息队列 API
    cat > docs/api/message-queue/README.md << 'EOF'
# 消息队列 API

## 概述

Helianthus 消息队列系统提供了高性能、可靠的消息传递功能。

## 核心类

### IMessageQueue

消息队列接口，定义了所有队列操作。

#### 主要方法

- `Initialize(Config)` - 初始化队列
- `SendMessage(QueueName, Message)` - 发送消息
- `ReceiveMessage(QueueName, Message)` - 接收消息
- `CreateQueue(Config)` - 创建队列
- `DeleteQueue(QueueName)` - 删除队列

#### 示例

```cpp
#include "Shared/MessageQueue/MessageQueue.h"

auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
Queue->Initialize();

QueueConfig Config;
Config.Name = "my_queue";
Config.Persistence = PersistenceMode::MEMORY_ONLY;
Queue->CreateQueue(Config);

auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
Message->Payload.Data.assign("Hello World", "Hello World" + 11);

Queue->SendMessage(Config.Name, Message);
```

### MessageQueue

消息队列的具体实现。

### 配置选项

#### QueueConfig

- `Name` - 队列名称
- `Persistence` - 持久化模式
- `MaxSize` - 最大队列大小
- `TimeoutMs` - 超时时间

#### CompressionConfig

- `Algorithm` - 压缩算法
- `Level` - 压缩级别
- `MinSize` - 最小压缩大小

#### EncryptionConfig

- `Algorithm` - 加密算法
- `Key` - 加密密钥
- `IV` - 初始化向量
EOF

    # 监控 API
    cat > docs/api/monitoring/README.md << 'EOF'
# 监控 API

## 概述

Helianthus 提供了完整的监控和指标收集功能。

## Prometheus 集成

### 指标导出

系统自动导出以下指标：

- `helianthus_queue_length` - 队列长度
- `helianthus_message_count` - 消息计数
- `helianthus_batch_duration_ms` - 批处理耗时
- `helianthus_zero_copy_duration_ms` - 零拷贝耗时
- `helianthus_transaction_commit_duration_ms` - 事务提交耗时

### 配置

```cpp
// 启动指标导出器
auto Exporter = std::make_unique<Helianthus::Monitoring::EnhancedPrometheusExporter>();
Exporter->Start("0.0.0.0", 9090);
```

## Grafana 仪表板

提供了预配置的 Grafana 仪表板，包含：

- 事务成功率
- 批处理性能
- 零拷贝操作统计
- 队列状态监控

### 导入仪表板

1. 启动 Grafana
2. 导入 `Docs/grafana_dashboard.json`
3. 配置 Prometheus 数据源
EOF

    # 事务 API
    cat > docs/api/transactions/README.md << 'EOF'
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
EOF
}

# 生成用户指南
generate_user_guides() {
    echo "📖 生成用户指南..."
    
    # 复制现有文档
    if [ -f "Docs/PerformanceBenchmarkReport.md" ]; then
        cp Docs/PerformanceBenchmarkReport.md docs/reports/
    fi
    
    if [ -f "Docs/DistributedTransactionValidationReport.md" ]; then
        cp Docs/DistributedTransactionValidationReport.md docs/reports/
    fi
    
    if [ -f "Docs/Grafana_Setup_Guide.md" ]; then
        cp Docs/Grafana_Setup_Guide.md docs/guides/
    fi
    
    # 创建快速开始指南
    cat > docs/guides/quick-start.md << 'EOF'
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
EOF
}

# 生成示例代码
generate_examples() {
    echo "💻 生成示例代码..."
    
    # 基本示例
    cat > docs/examples/basic-usage.cpp << 'EOF'
#include "Shared/MessageQueue/MessageQueue.h"
#include <iostream>

int main() {
    // 初始化队列
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    // 创建队列
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "example_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Config.MaxSize = 1000;
    Queue->CreateQueue(Config);
    
    // 发送消息
    for (int i = 0; i < 5; ++i) {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "Message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        
        Queue->SendMessage(Config.Name, Message);
        std::cout << "Sent: " << Payload << std::endl;
    }
    
    // 接收消息
    std::cout << "\nReceiving messages:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        Helianthus::MessageQueue::MessagePtr ReceivedMessage;
        Queue->ReceiveMessage(Config.Name, ReceivedMessage);
        
        std::string Payload(ReceivedMessage->Payload.Data.begin(), 
                           ReceivedMessage->Payload.Data.end());
        std::cout << "Received: " << Payload << std::endl;
    }
    
    return 0;
}
EOF

    # 事务示例
    cat > docs/examples/transactions.cpp << 'EOF'
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
EOF

    # 监控示例
    cat > docs/examples/monitoring.cpp << 'EOF'
#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // 启动监控
    auto Exporter = std::make_unique<Helianthus::Monitoring::EnhancedPrometheusExporter>();
    Exporter->Start("0.0.0.0", 9090);
    std::cout << "监控服务启动在 http://localhost:9090/metrics" << std::endl;
    
    // 创建队列并产生一些活动
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "monitor_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);
    
    // 模拟一些活动
    for (int i = 0; i < 10; ++i) {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "Monitor message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        
        Queue->SendMessage(Config.Name, Message);
        
        // 获取性能统计
        Helianthus::MessageQueue::PerformanceStats Stats;
        Queue->GetPerformanceStats(Stats);
        
        std::cout << "消息 " << i << " - 平均批处理时间: " 
                  << Stats.AverageBatchTimeMs << "ms" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n监控数据已生成，请访问 http://localhost:9090/metrics" << std::endl;
    std::cout << "按 Ctrl+C 退出..." << std::endl;
    
    // 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
EOF
}

# 创建文档首页
create_index() {
    echo "🏠 创建文档首页..."
    cat > docs/README.md << 'EOF'
# Helianthus 文档

## 📚 API 文档

- [消息队列 API](api/message-queue/README.md) - 消息队列核心功能
- [监控 API](api/monitoring/README.md) - 监控和指标收集
- [事务 API](api/transactions/README.md) - 分布式事务功能

## 🚀 快速开始

- [5分钟上手](guides/quick-start.md) - 快速开始指南
- [安装指南](guides/installation.md) - 详细安装说明
- [配置说明](guides/configuration.md) - 配置选项详解
- [部署指南](guides/deployment.md) - 生产环境部署

## 💻 示例代码

- [基本用法](examples/basic-usage.cpp) - 基础消息队列操作
- [事务处理](examples/transactions.cpp) - 事务示例
- [监控集成](examples/monitoring.cpp) - 监控示例

## 📊 性能报告

- [性能基准测试](reports/PerformanceBenchmarkReport.md) - 性能测试结果
- [分布式事务验证](reports/DistributedTransactionValidationReport.md) - 事务功能验证

## 🔧 开发资源

- [架构设计](guides/architecture.md) - 系统架构说明
- [扩展开发](guides/extension.md) - 如何扩展功能
- [测试指南](guides/testing.md) - 测试最佳实践

## 📈 监控和运维

- [Grafana 设置](guides/Grafana_Setup_Guide.md) - 监控仪表板配置
- [性能调优](guides/performance.md) - 性能优化建议
- [故障排查](guides/troubleshooting.md) - 常见问题解决

---

**最后更新**: 2025年09月  
**版本**: 1.0  
**构建**: Bazel 6.0+  
**编译器**: GCC 9+ / Clang 10+ / MSVC 2019+
EOF
}

# 主函数
main() {
    create_directories
    generate_api_docs
    generate_user_guides
    generate_examples
    create_index
    
    echo "🎉 文档生成完成！"
    echo "📖 文档位置: docs/"
    echo "📖 首页: docs/README.md"
    echo "📖 API 文档: docs/api/"
    echo "📖 用户指南: docs/guides/"
    echo "💻 示例代码: docs/examples/"
    echo "📊 性能报告: docs/reports/"
}

main "$@"
