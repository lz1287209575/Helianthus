# Helianthus Examples

## 性能监控示例 performance_monitoring_example

构建与运行：

```
bazel build //Examples:performance_monitoring_example
./bazel-bin/Examples/performance_monitoring_example
```

运行后输出：
- 控制台展示连接/操作级统计摘要（平均延迟、分位延迟、成功率等）
- 末尾打印 Prometheus 文本格式的前若干行示例

集成建议：
- 在你的服务中提供 `/metrics` HTTP 路由，返回：
  ```cpp
  auto& Monitor = Helianthus::Network::Asio::PerformanceMonitor::Instance();
  std::string body = Monitor.ExportPrometheusMetrics();
  // write body to response
  ```
- Prometheus 抓取该HTTP路径即可采集指标

## 消息队列示例

### dlq_monitor_test - 死信队列监控测试

**构建和运行：**
```bash
bazel build //Examples:dlq_monitor_test
bazel run //Examples:dlq_monitor_test
```

**功能演示：**
- 死信队列系统完整功能
- 消息过期、拒收、重试机制
- DLQ监控和告警
- 结构化日志和线程本地上下文
- 队列指标监控
- 集群/HA功能演示

**配置指标监控：**
```cpp
Queue->SetGlobalConfig("metrics.interval.ms", "2000");
Queue->SetGlobalConfig("metrics.window.ms", "60000");
Queue->SetGlobalConfig("metrics.latency.capacity", "1024");
```

**查看结构化日志：**
```bash
# 注意：由于 Bazel 沙盒限制，日志文件可能不在预期位置
# 可以查看 bazel-bin/Examples/ 目录下的日志文件
```

**Prometheus 集成建议：**
- 将 `GetQueueMetrics` 输出格式化为 Prometheus 指标
- 添加 `/metrics` HTTP 端点
- 配置 Prometheus 抓取规则

**查看分片状态：**
```cpp
std::vector<ShardInfo> ShardStatuses;
Queue->GetClusterShardStatuses(ShardStatuses);
for (const auto& Shard : ShardStatuses) {
    // 显示分片状态
}
```

### simple_ha_test - 简单 HA 故障转移测试

**构建和运行：**
```bash
bazel build //Examples:simple_ha_test
./bazel-bin/Examples/simple_ha_test
```

**功能演示：**
- 集群配置和分片设置
- 健康感知路由
- 节点故障模拟
- 自动故障转移
- Leader 选举
- 分片状态监控

**测试场景：**
1. **正常消息发送** - 验证基本功能
2. **节点故障模拟** - 设置 node-b 为不健康状态
3. **故障转移验证** - 观察自动 Leader 变更
4. **分片状态查看** - 检查集群健康状态

**预期输出：**
```
=== 简单 HA 测试开始 ===
创建消息队列实例
开始初始化消息队列...
消息队列初始化成功
集群配置设置完成: 2个分片，每个分片2个副本
心跳波动概率设置为 0.1
创建队列成功: ha_test_queue
=== 演示1：正常消息发送 ===
发送消息成功: id=1, partition_key=user_1
发送消息成功: id=2, partition_key=user_0
发送消息成功: id=3, partition_key=user_1
=== 演示2：模拟节点故障 ===
设置 node-b 为不健康状态
发送消息成功: id=4, partition_key=user_0
发送消息成功: id=5, partition_key=user_1
发送消息成功: id=6, partition_key=user_0
=== 演示3：查看分片状态 ===
分片状态: shard=0, leader=node-a(健康), healthy_followers=0
分片状态: shard=1, leader=node-b(不健康), healthy_followers=1
等待5秒观察心跳...
Failover发生: shard=1, failed_leader=node-b, takeover=node-a
Leader变更: shard=1, old=node-b, new=node-a
=== 简单 HA 测试完成 ===
```

### ha_failover_demo - 完整 HA 故障转移演示

**构建和运行：**
```bash
bazel build //Examples:ha_failover_demo
./bazel-bin/Examples/ha_failover_demo
```

**功能演示：**
- 完整的 HA 故障转移流程
- 结构化日志记录
- 线程本地上下文
- 队列指标监控
- 复制指标输出
- 故障转移回调处理

**配置选项：**
```cpp
// 心跳波动概率（增加故障模拟频率）
Queue->SetGlobalConfig("cluster.heartbeat.flap.prob", "0.1");

// 最小复制确认数
Queue->SetGlobalConfig("replication.min.acks", "1");
```

**故障转移回调：**
```cpp
Queue->SetLeaderChangeHandler([](ShardId Shard, const std::string& OldLeader, const std::string& NewLeader) {
    // 处理 Leader 变更事件
});

Queue->SetFailoverHandler([](ShardId Shard, const std::string& FailedLeader, const std::string& TakeoverNode) {
    // 处理故障转移事件
});
```

### transaction_test - 事务功能测试

**构建和运行：**
```bash
bazel build //Examples:transaction_test
./bazel-bin/Examples/transaction_test
```

**功能演示：**
- 事务创建和管理
- 事务内消息操作
- 事务提交和回滚
- 事务统计和监控
- 事务回调处理

**测试场景：**
1. **成功事务** - 创建事务，发送消息，提交事务
2. **回滚事务** - 创建事务，发送消息，回滚事务
3. **事务统计** - 查看事务成功率、回滚率等统计信息
4. **事务状态** - 查询事务状态

**预期输出：**
```
=== 事务功能测试开始 ===
创建消息队列实例
开始初始化消息队列...
消息队列初始化成功
创建队列成功: transaction_test_queue
=== 测试1：成功的事务 ===
开始事务: id=1
事务内发送消息成功: id=0
事务提交回调: id=1, success=true, error=
事务提交成功: id=1
=== 测试2：回滚的事务 ===
开始事务: id=2
事务内发送消息成功: id=0
事务回滚回调: id=2, reason=测试回滚
事务回滚成功: id=2
=== 测试3：查看事务统计 ===
事务统计:
  总事务数: 2
  已提交: 1
  已回滚: 1
  超时: 0
  失败: 0
  成功率: 50%
  回滚率: 50%
  平均提交时间: 0ms
  平均回滚时间: 0ms
=== 测试4：查看事务状态 ===
事务1状态: 1
事务2状态: 2
等待5秒观察事务超时...
=== 事务功能测试完成 ===
```

**事务API使用：**
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

// 查询事务统计
TransactionStats Stats;
Queue->GetTransactionStats(Stats);
```

