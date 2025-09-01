# 快速开始

## 🚀 5分钟上手 Helianthus

本指南将帮助您在5分钟内搭建并运行第一个 Helianthus 应用。

## 📋 系统要求

- **操作系统**: Linux / macOS / Windows
- **编译器**: GCC 9+ / Clang 10+ / MSVC 2019+
- **构建系统**: Bazel 6.0+
- **内存**: 4GB+ (推荐 8GB+)
- **磁盘**: 1GB+ 可用空间

## ⚡ 快速安装

### 1. 克隆项目

```bash
git clone https://github.com/lz1287209575/helianthus.git
cd helianthus
```

### 2. 安装 Bazel

**Ubuntu/Debian:**
```bash
sudo apt update && sudo apt install bazel
```

**macOS:**
```bash
brew install bazel
```

**Windows:**
```bash
# 下载并安装 Bazel from https://bazel.build/install
```

### 3. 构建项目

```bash
# 构建所有组件
bazel build //...

# 构建特定组件
bazel build //Shared/MessageQueue:message_queue
```

### 4. 运行测试

```bash
# 运行所有测试
bazel test //...

# 运行特定测试
bazel test //Tests/Message:all
```

## 💻 第一个应用

### 基本消息队列

创建一个简单的消息队列应用：

```cpp
#include "Shared/MessageQueue/MessageQueue.h"
#include <iostream>

int main() {
    // 创建队列
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    // 创建队列配置
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "hello_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);
    
    // 发送消息
    auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
    Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
    Message->Payload.Data.assign("Hello Helianthus!", "Hello Helianthus!" + 17);
    Queue->SendMessage(Config.Name, Message);
    
    std::cout << "✅ 消息发送成功!" << std::endl;
    
    // 接收消息
    Helianthus::MessageQueue::MessagePtr ReceivedMessage;
    Queue->ReceiveMessage(Config.Name, ReceivedMessage);
    
    std::string Payload(ReceivedMessage->Payload.Data.begin(), 
                       ReceivedMessage->Payload.Data.end());
    std::cout << "📨 收到消息: " << Payload << std::endl;
    
    return 0;
}
```

### 编译和运行

```bash
# 编译示例
bazel build //Examples:basic_example

# 运行示例
bazel run //Examples:basic_example
```

## 🔄 事务处理

### 基本事务

```cpp
// 开始事务
TransactionId TxId = Queue->BeginTransaction("my_transaction", 30000);

// 在事务中发送消息
auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
Message->Payload.Data.assign("transaction message", "transaction message" + 20);
Queue->SendMessageInTransaction(TxId, "hello_queue", Message);

// 提交事务
Queue->CommitTransaction(TxId);
std::cout << "✅ 事务提交成功!" << std::endl;
```

## 📊 监控集成

### 启动监控

```cpp
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"

int main() {
    // 启动监控服务
    auto Exporter = std::make_unique<Helianthus::Monitoring::EnhancedPrometheusExporter>();
    Exporter->Start("0.0.0.0", 9090);
    
    std::cout << "📊 监控服务启动在 http://localhost:9090/metrics" << std::endl;
    
    // 您的应用代码...
    
    return 0;
}
```

### 访问指标

- **Prometheus 指标**: http://localhost:9090/metrics
- **Grafana 仪表板**: 导入 `Docs/grafana_dashboard.json`

## 🧪 运行示例

项目包含多个示例应用：

```bash
# 基本消息队列示例
bazel run //Examples:basic_example

# 事务处理示例
bazel run //Examples:transaction_example

# 监控示例
bazel run //Examples:enhanced_prometheus_example

# 性能测试
bazel test //Tests/Message:performance_benchmark_test

# 分布式事务测试
bazel test //Tests/Message:distributed_transaction_test
```

## 🔧 常见问题

### Q: 编译失败怎么办？
A: 确保安装了正确版本的 Bazel 和编译器，检查系统要求。

### Q: 测试失败怎么办？
A: 运行 `bazel test //... --test_output=all` 查看详细错误信息。

### Q: 如何查看性能指标？
A: 启动监控服务后访问 http://localhost:9090/metrics

### Q: 如何配置持久化？
A: 修改 `QueueConfig.Persistence` 为 `PersistenceMode::FILE_BASED`

## 📚 下一步

- [API 参考](API-Reference) - 完整的 API 文档
- [配置说明](Configuration) - 详细配置选项
- [部署指南](Deployment) - 生产环境部署
- [示例代码](Examples) - 更多代码示例

## 🆘 需要帮助？

- 查看 [常见问题](FAQ)
- 提交 [GitHub Issue](https://github.com/lz1287209575/helianthus/issues)
- 参与 [GitHub Discussions](https://github.com/lz1287209575/helianthus/discussions)

---

**提示**: 建议先运行基本示例熟悉 API，然后再尝试更复杂的功能。
