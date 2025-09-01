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
