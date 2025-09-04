# Helianthus 游戏服务器

[![Build Status]([![Helianthus CI/CD Pipeline](https://github.com/lz1287209575/Helianthus/actions/workflows/ci.yml/badge.svg)](https://github.com/lz1287209575/Helianthus/actions/workflows/ci.yml))
[![C++](https://img.shields.io/badge/C%2B%2B-17%2F20-blue)](https://isocpp.org/)
[![Bazel](https://img.shields.io/badge/build-Bazel-green)](https://bazel.build/)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

Helianthus 是一个高性能、可扩展的微服务游戏服务器架构，采用现代C++开发，支持多种脚本语言集成。

## 🌟 项目特色

- **🏗️ 微服务架构**: 模块化设计，支持独立部署和扩展
- **⚡ 高性能**: 自研网络层、消息队列和服务发现系统
- **🔧 多语言脚本**: 支持 Lua、Python、JavaScript、C# 脚本集成
- **🎯 现代C++**: 基于 C++17/20 标准，性能与安全并重
- **🚀 企业级**: 内置负载均衡、健康检查、故障转移等功能
- **🔄 运行时反射**: 类似UE的反射系统，支持热更新和编辑器开发

## 📋 系统要求

- **编译器**: GCC 9+ / Clang 10+ / MSVC 2019+
- **构建系统**: Bazel 6.0+
- **操作系统**: Linux / macOS / Windows
- **内存**: 建议 8GB+
- **磁盘**: 2GB+ (包含依赖)

## 🚀 快速开始

### 1. 克隆项目
```bash
git clone https://github.com/lz1287209575/helianthus.git
cd helianthus
```

### 2. 安装 Bazel
```bash
# Ubuntu/Debian
sudo apt install bazel

# macOS
brew install bazel

# Windows
# 下载并安装 Bazel from https://bazel.build/install
```

### 3. 构建项目
```bash
# 构建所有组件
bazel build //...

# 构建特定组件
bazel build //Shared/Network:network

# 启用脚本支持构建
bazel build //... --define=ENABLE_LUA_SCRIPTING=1
```

### 4. 运行测试
```bash
# 运行所有测试
bazel test //...

# 运行特定测试
bazel test //Tests/Network:all
```

## 📁 项目结构

```
Helianthus/
├── Services/                    # 微服务模块
│   ├── Gateway/                # 网关服务
│   ├── Auth/                   # 认证服务
│   ├── Game/                   # 游戏逻辑服务
│   ├── Player/                 # 玩家管理服务
│   ├── Match/                  # 匹配服务
│   ├── Realtime/              # 实时通信服务
│   └── Monitor/               # 监控服务
├── Shared/                     # 共享组件
│   ├── Network/               # 自定义网络层
│   ├── Message/               # 消息队列系统
│   ├── Discovery/             # 服务发现
│   ├── Protocol/              # 协议定义
│   ├── Reflection/            # 反射系统
│   ├── Scripting/             # 脚本引擎
│   └── Common/                # 通用工具
├── Tools/                      # 开发工具
│   ├── BuildSystem/           # 自定义构建工具
│   └── Monitoring/            # 监控工具
├── ThirdParty/                # 第三方依赖
├── Tests/                     # 单元测试
└── Docs/                      # 文档
```

## 🏗️ 核心架构

### 网络层 (Network)
- **自定义实现**: 高性能 TCP/UDP/WebSocket 支持
- **连接池管理**: 智能连接复用和管理
- **异步IO**: 基于事件驱动的异步网络处理

### 消息队列 (Message)
- **优先级队列**: 支持消息优先级和路由
- **可靠传输**: 消息确认和重传机制
- **热更新支持**: 运行时消息格式变更

#### 指标系统（Queue Metrics）
- 基础指标：队列长度、累计发送/接收/确认/重试/死信计数
- 速率指标：入/出队速率，基于可配置滑动窗口计算
- 时延指标：处理时延 P50/P95（样本近似）
- 周期输出：结构化日志定期输出（文件 sink），控制台可关闭

配置示例：
```cpp
auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
Queue->Initialize();

// 指标采集与输出
Queue->SetGlobalConfig("metrics.interval.ms", "2000");        // 指标输出间隔（毫秒）
Queue->SetGlobalConfig("metrics.window.ms", "60000");         // 滑动窗口（毫秒）
Queue->SetGlobalConfig("metrics.latency.capacity", "1024");  // 时延样本容量

// 查询单队列指标
Helianthus::MessageQueue::QueueMetrics m;
Queue->GetQueueMetrics("queue_name", m);
```

## 集群和分片（一致性哈希）

### 路由策略
- **分区键优先**：消息属性 `partition_key` 优先用于路由
- **队列名回退**：无分区键时使用队列名进行路由
- **一致性哈希**：基于虚拟节点的一致性哈希分片

### 配置项
```cpp
// 设置分片数量
Queue->SetGlobalConfig("cluster.shards", "4");

// 设置每个分片的虚拟节点数
Queue->SetGlobalConfig("cluster.shard.vnodes", "150");

// 设置心跳波动概率（用于测试）
Queue->SetGlobalConfig("cluster.heartbeat.flap.prob", "0.1");
```

### 副本/HA
- **Leader/Follower 角色**：每个分片支持多个副本
- **健康状态管理**：动态健康检查
- **自动故障转移**：Leader 故障时自动选举新 Leader
- **健康感知路由**：优先选择健康的 Leader，回退到健康的 Follower

### 心跳与健康
- **轻量级心跳**：定期健康检查和状态更新
- **模拟健康波动**：可配置的故障模拟概率
- **Leader 选举**：基于健康状态的自动选举

### 分片状态导出
```cpp
std::vector<ShardInfo> ShardStatuses;
Queue->GetClusterShardStatuses(ShardStatuses);
for (const auto& Shard : ShardStatuses) {
    // 显示每个分片的 Leader 和健康 Follower 数量
}
```

### 副本同步骨架
- **WAL 条目**：每个分片维护 WAL 日志
- **Follower 位点**：跟踪每个 Follower 的应用进度
- **复制确认**：可配置的最小复制确认数
- **复制指标**：复制滞后度和 ACK 计数

### 故障转移回调
```cpp
// Leader 变更回调
Queue->SetLeaderChangeHandler([](ShardId Shard, const std::string& OldLeader, const std::string& NewLeader) {
    // 处理 Leader 变更事件
});

// 故障转移回调
Queue->SetFailoverHandler([](ShardId Shard, const std::string& FailedLeader, const std::string& TakeoverNode) {
    // 处理故障转移事件
});
```

### 路由容错增强
- **自动回退**：主节点失效时自动回退到备用节点
- **重试幂等**：支持消息重试，避免重复处理
- **健康感知**：路由时考虑节点健康状态
- **故障检测**：实时检测节点故障并触发故障转移

## 事务支持

### 事务管理
- **事务创建**：支持本地事务和分布式事务
- **事务操作**：在事务内执行消息发送、确认、拒收、队列创建/删除
- **事务提交**：原子性提交所有事务操作
- **事务回滚**：支持事务失败时的完整回滚

### 事务API
```cpp
// 开始事务
TransactionId TxId = Queue->BeginTransaction("事务描述", 10000);

// 事务内操作
Queue->SendMessageInTransaction(TxId, "queue_name", Message);
Queue->AcknowledgeMessageInTransaction(TxId, "queue_name", MessageId);
Queue->RejectMessageInTransaction(TxId, "queue_name", MessageId, "原因");
Queue->CreateQueueInTransaction(TxId, QueueConfig);
Queue->DeleteQueueInTransaction(TxId, "queue_name");

// 提交或回滚事务
Queue->CommitTransaction(TxId);
Queue->RollbackTransaction(TxId, "回滚原因");
```

### 事务监控
- **事务统计**：总事务数、成功率、回滚率、超时率
- **性能指标**：平均提交时间、平均回滚时间
- **事务回调**：提交、回滚、超时事件回调

### 分布式事务
- **两阶段提交**：Prepare 和 Commit 阶段
- **协调者模式**：支持分布式事务协调
- **故障恢复**：分布式事务的故障检测和恢复

### 事务回调
```cpp
// 设置事务回调
Queue->SetTransactionCommitHandler([](TransactionId Id, bool Success, const std::string& ErrorMessage) {
    // 处理事务提交结果
});

Queue->SetTransactionRollbackHandler([](TransactionId Id, const std::string& Reason) {
    // 处理事务回滚
});

Queue->SetTransactionTimeoutHandler([](TransactionId Id) {
    // 处理事务超时
});
```

## 消息压缩和加密

### 压缩算法支持
- **GZIP**：通用压缩算法，平衡压缩率和速度
- **LZ4**：高速压缩算法，适合实时应用
- **ZSTD**：Facebook 开发的高效压缩算法
- **Snappy**：Google 开发的快速压缩算法

### 加密算法支持
- **AES-256-GCM**：高级加密标准，带认证
- **ChaCha20-Poly1305**：现代流密码，高性能
- **AES-128-CBC**：传统加密模式

### 压缩和加密配置
```cpp
// 设置压缩配置
CompressionConfig CompConfig;
CompConfig.Algorithm = CompressionAlgorithm::GZIP;
CompConfig.Level = 6;              // 压缩级别 (1-9)
CompConfig.MinSize = 1024;         // 最小压缩大小
CompConfig.EnableAutoCompression = true;
Queue->SetCompressionConfig("queue_name", CompConfig);

// 设置加密配置
EncryptionConfig EncConfig;
EncConfig.Algorithm = EncryptionAlgorithm::AES_256_GCM;
EncConfig.Key = "your-secret-key-32-bytes-long";
EncConfig.IV = "your-iv-16-bytes";
EncConfig.EnableAutoEncryption = true;
Queue->SetEncryptionConfig("queue_name", EncConfig);
```

### 手动压缩和加密
```cpp
// 手动压缩消息
Queue->CompressMessage(Message, CompressionAlgorithm::GZIP);

// 手动解压消息
Queue->DecompressMessage(Message);

// 手动加密消息
Queue->EncryptMessage(Message, EncryptionAlgorithm::AES_256_GCM);

// 手动解密消息
Queue->DecryptMessage(Message);
```

### 压缩和加密统计
```cpp
// 获取压缩统计
CompressionStats CompStats;
Queue->GetCompressionStats("queue_name", CompStats);
// CompStats.CompressionRatio - 压缩比
// CompStats.AverageCompressionTimeMs - 平均压缩时间

// 获取加密统计
EncryptionStats EncStats;
Queue->GetEncryptionStats("queue_name", EncStats);
// EncStats.AverageEncryptionTimeMs - 平均加密时间
// EncStats.AverageDecryptionTimeMs - 平均解密时间
```

## 监控告警

### 告警级别和类型
- **告警级别**：INFO、WARNING、ERROR、CRITICAL
- **告警类型**：队列满、队列空、高延迟、低吞吐量、死信队列、消费者离线、磁盘空间、内存使用率、CPU使用率、网络错误、持久化错误、压缩错误、加密错误、事务超时、复制滞后、节点健康、自定义告警

### 告警配置管理
```cpp
// 设置告警配置
AlertConfig AlertConfig;
AlertConfig.Type = AlertType::QUEUE_FULL;
AlertConfig.Level = AlertLevel::WARNING;
AlertConfig.QueueName = "queue_name";
AlertConfig.Threshold = 0.8;              // 80% 使用率时告警
AlertConfig.DurationMs = 60000;           // 1分钟持续时间
AlertConfig.CooldownMs = 300000;          // 5分钟冷却时间
AlertConfig.Enabled = true;
AlertConfig.Description = "队列使用率过高告警";
AlertConfig.NotifyChannels = {"email", "slack"};
Queue->SetAlertConfig(AlertConfig);

// 查询告警配置
AlertConfig RetrievedConfig;
Queue->GetAlertConfig(AlertType::QUEUE_FULL, "queue_name", RetrievedConfig);
```

### 告警查询和管理
```cpp
// 查询活跃告警
std::vector<Alert> ActiveAlerts;
Queue->GetActiveAlerts(ActiveAlerts);

// 查询告警历史
std::vector<Alert> AlertHistory;
Queue->GetAlertHistory(10, AlertHistory);

// 查询告警统计
AlertStats AlertStats;
Queue->GetAlertStats(AlertStats);
// AlertStats.TotalAlerts - 总告警数
// AlertStats.ActiveAlerts - 活跃告警数
// AlertStats.WarningAlerts - 警告级别告警数

// 确认告警
Queue->AcknowledgeAlert(AlertId);

// 解决告警
Queue->ResolveAlert(AlertId, "问题已解决");

// 清空所有告警
Queue->ClearAllAlerts();
```

### 告警回调处理
```cpp
// 设置告警处理器
Queue->SetAlertHandler([](const Alert& Alert) {
    std::cout << "收到告警: id=" << Alert.Id 
              << ", type=" << static_cast<int>(Alert.Type)
              << ", level=" << static_cast<int>(Alert.Level)
              << ", message=" << Alert.Message << std::endl;
});

// 设置告警配置变更处理器
Queue->SetAlertConfigHandler([](const AlertConfig& Config) {
    std::cout << "告警配置变更: type=" << static_cast<int>(Config.Type)
              << ", queue=" << Config.QueueName << std::endl;
});
```

## 性能优化

### 内存池管理
```cpp
// 设置内存池配置
MemoryPoolConfig MemoryPoolConfig;
MemoryPoolConfig.InitialSize = 1024 * 1024;        // 1MB
MemoryPoolConfig.MaxSize = 100 * 1024 * 1024;      // 100MB
MemoryPoolConfig.BlockSize = 4096;                 // 4KB
MemoryPoolConfig.GrowthFactor = 2;                 // 增长因子
MemoryPoolConfig.EnablePreallocation = true;       // 启用预分配
MemoryPoolConfig.PreallocationBlocks = 1000;       // 预分配1000个块
MemoryPoolConfig.EnableCompaction = true;          // 启用内存压缩
MemoryPoolConfig.CompactionThreshold = 50;         // 50%压缩阈值
Queue->SetMemoryPoolConfig(MemoryPoolConfig);

// 从内存池分配内存
void* Ptr = nullptr;
Queue->AllocateFromPool(1024, Ptr);

// 释放内存到内存池
Queue->DeallocateToPool(Ptr, 1024);

// 压缩内存池
Queue->CompactMemoryPool();
```

### 缓冲区优化
```cpp
// 设置缓冲区配置
BufferConfig BufferConfig;
BufferConfig.InitialCapacity = 8192;               // 8KB
BufferConfig.MaxCapacity = 1024 * 1024;            // 1MB
BufferConfig.GrowthFactor = 2;                     // 增长因子
BufferConfig.EnableZeroCopy = true;                // 启用零拷贝
BufferConfig.EnableCompression = false;            // 禁用压缩
BufferConfig.CompressionThreshold = 1024;          // 压缩阈值
BufferConfig.EnableBatching = true;                // 启用批处理
BufferConfig.BatchSize = 100;                      // 批处理大小
BufferConfig.BatchTimeoutMs = 100;                 // 批处理超时时间
Queue->SetBufferConfig(BufferConfig);
```

### 零拷贝操作
```cpp
// 创建零拷贝缓冲区
std::string Data = "零拷贝测试数据";
ZeroCopyBuffer Buffer;
Queue->CreateZeroCopyBuffer(Data.data(), Data.size(), Buffer);

// 零拷贝发送消息
Queue->SendMessageZeroCopy("queue_name", Buffer);

// 释放零拷贝缓冲区
Queue->ReleaseZeroCopyBuffer(Buffer);
```

### 批处理操作
```cpp
// 创建批处理
uint32_t BatchId = 0;
Queue->CreateBatch(BatchId);

// 添加消息到批处理
auto Message = std::make_shared<Message>();
Message->Payload.Data = std::vector<char>("消息内容".begin(), "消息内容".end());
Queue->AddToBatch(BatchId, Message);

// 提交批处理
Queue->CommitBatch(BatchId);

// 获取批处理信息
BatchMessage BatchInfo;
Queue->GetBatchInfo(BatchId, BatchInfo);
```

### 性能统计
```cpp
// 获取性能统计
PerformanceStats Stats;
Queue->GetPerformanceStats(Stats);
// Stats.TotalAllocations - 总分配次数
// Stats.MemoryPoolHitRate - 内存池命中率
// Stats.ZeroCopyOperations - 零拷贝操作次数
// Stats.BatchOperations - 批处理操作次数
// Stats.AverageAllocationTimeMs - 平均分配时间

// 重置性能统计
Queue->ResetPerformanceStats();
```

### 服务发现 (Discovery)
- **服务注册**: 自动服务注册和注销
- **负载均衡**: 多种负载均衡策略
- **健康检查**: 实时服务健康监控
- **故障转移**: 自动故障检测和转移

### 反射系统 (Reflection)
- **运行时反射**: 类、属性、方法的运行时信息
- **序列化集成**: 自动序列化和反序列化
- **脚本绑定**: 自动生成脚本语言绑定
- **编辑器支持**: 运行时检查和修改

### 脚本引擎 (Scripting)
- **多语言支持**: Lua/Python/JavaScript/C#
- **热更新**: 无需重启的脚本更新
- **沙箱执行**: 安全的脚本执行环境
- **性能优化**: JIT编译和缓存机制

## 🛠️ 开发指南

### 编码规范
- **命名规范**: 
  - 变量: `UPPER_CASE`
  - 函数/类: `PascalCase`
  - 参数: `PascalCase`
  - 目录: `PascalCase`
- **代码格式**: 使用 `clang-format` 自动格式化
- **接口设计**: 优先使用抽象接口，支持依赖注入

### 构建命令
```bash
# 格式化代码
clang-format -i **/*.{cpp,hpp,h}

# 清理构建
bazel clean

# 调试构建
bazel build //... --compilation_mode=dbg

# 发布构建
bazel build //... --compilation_mode=opt
```

### 脚本语言配置
```bash
# 启用 Lua
bazel build //... --define=ENABLE_LUA_SCRIPTING=1

# 启用 Python
bazel build //... --define=ENABLE_PYTHON_SCRIPTING=1

# 启用 JavaScript
bazel build //... --define=ENABLE_JS_SCRIPTING=1

# 启用 C#/.NET
bazel build //... --define=ENABLE_DOTNET_SCRIPTING=1

# 启用多个脚本引擎
bazel build //... \
  --define=ENABLE_LUA_SCRIPTING=1 \
  --define=ENABLE_PYTHON_SCRIPTING=1
```

## 🔧 配置说明

### 网络配置
```cpp
NetworkConfig config;
config.MaxConnections = 10000;
config.BufferSize = 8192;
config.TimeoutMs = 30000;
config.EnableKeepalive = true;
```

### 消息队列配置
```cpp
MessageQueueConfig config;
config.MaxQueueSize = 10000;
config.MaxMessageSize = 1024 * 1024; // 1MB
config.EnablePersistence = true;
config.EnableCompression = true;
```

### 服务发现配置
```cpp
RegistryConfig config;
config.MaxServices = 1000;
config.DefaultTtlMs = 300000; // 5分钟
config.EnableReplication = true;
```

## 📈 开发路线图

### Phase 1: 核心基础架构 ✅
- [x] 项目基础设施和构建系统
- [x] 网络层接口设计
- [x] 消息队列架构
- [x] 服务发现系统

### Phase 1.5: 反射和脚本 🚧
- [ ] 运行时反射系统实现
- [ ] 多语言脚本引擎集成
- [ ] 自动绑定生成器
- [ ] 热更新支持

### Phase 2: 基础服务 📋
- [ ] 网关服务实现
- [ ] 认证服务实现
- [ ] 玩家管理服务
- [ ] 基础游戏逻辑框架

### Phase 3: 高级功能 📋
- [ ] 实时通信优化
- [ ] 游戏逻辑热更新
- [ ] 分布式缓存系统
- [ ] 性能监控和分析

### Phase 4: 生产特性 📋
- [ ] 自定义监控系统
- [ ] 自定义日志系统
- [ ] 容器化和编排
- [ ] 可视化管理界面

## 🤝 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`
