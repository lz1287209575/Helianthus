# Helianthus 增强 Prometheus 指标导出功能

## 概述

Helianthus 框架现在提供了增强的 Prometheus 指标导出功能，包括批处理、零拷贝和事务的详细性能指标，支持直方图和分位数统计。

## 新增功能

### 1. 批处理性能指标

#### 直方图指标
- `helianthus_batch_duration_ms_bucket` - 批处理耗时分布直方图
- `helianthus_batch_duration_ms_sum` - 批处理总耗时
- `helianthus_batch_duration_ms_count` - 批处理总次数

#### 分位数指标
- `helianthus_batch_duration_p50_ms` - 批处理耗时 P50 分位数
- `helianthus_batch_duration_p95_ms` - 批处理耗时 P95 分位数
- `helianthus_batch_duration_p99_ms` - 批处理耗时 P99 分位数
- `helianthus_batch_duration_avg_ms` - 批处理平均耗时

#### 计数器指标
- `helianthus_batch_count_total` - 总批处理次数
- `helianthus_batch_messages_total` - 总批处理消息数

### 2. 零拷贝性能指标

#### 直方图指标
- `helianthus_zero_copy_duration_ms_bucket` - 零拷贝操作耗时分布直方图
- `helianthus_zero_copy_duration_ms_sum` - 零拷贝操作总耗时
- `helianthus_zero_copy_duration_ms_count` - 零拷贝操作总次数

#### 分位数指标
- `helianthus_zero_copy_duration_p50_ms` - 零拷贝操作耗时 P50 分位数
- `helianthus_zero_copy_duration_p95_ms` - 零拷贝操作耗时 P95 分位数
- `helianthus_zero_copy_duration_p99_ms` - 零拷贝操作耗时 P99 分位数
- `helianthus_zero_copy_duration_avg_ms` - 零拷贝操作平均耗时

#### 计数器指标
- `helianthus_zero_copy_operations_total` - 总零拷贝操作次数

### 3. 事务性能指标

#### 提交时间直方图
- `helianthus_transaction_commit_duration_ms_bucket` - 事务提交耗时分布直方图
- `helianthus_transaction_commit_duration_ms_sum` - 事务提交总耗时
- `helianthus_transaction_commit_duration_ms_count` - 事务提交总次数

#### 回滚时间直方图
- `helianthus_transaction_rollback_duration_ms_bucket` - 事务回滚耗时分布直方图
- `helianthus_transaction_rollback_duration_ms_sum` - 事务回滚总耗时
- `helianthus_transaction_rollback_duration_ms_count` - 事务回滚总次数

#### 分位数指标
- `helianthus_transaction_commit_duration_p50_ms` - 事务提交耗时 P50 分位数
- `helianthus_transaction_commit_duration_p95_ms` - 事务提交耗时 P95 分位数
- `helianthus_transaction_commit_duration_p99_ms` - 事务提交耗时 P99 分位数
- `helianthus_transaction_commit_duration_avg_ms` - 事务提交平均耗时

- `helianthus_transaction_rollback_duration_p50_ms` - 事务回滚耗时 P50 分位数
- `helianthus_transaction_rollback_duration_p95_ms` - 事务回滚耗时 P95 分位数
- `helianthus_transaction_rollback_duration_p99_ms` - 事务回滚耗时 P99 分位数
- `helianthus_transaction_rollback_duration_avg_ms` - 事务回滚平均耗时

#### 计数器指标
- `helianthus_transaction_total` - 总事务数
- `helianthus_transaction_committed_total` - 已提交事务数
- `helianthus_transaction_rolled_back_total` - 已回滚事务数
- `helianthus_transaction_timeout_total` - 超时事务数
- `helianthus_transaction_failed_total` - 失败事务数

#### 比率指标
- `helianthus_transaction_success_rate` - 事务成功率
- `helianthus_transaction_rollback_rate` - 事务回滚率
- `helianthus_transaction_timeout_rate` - 事务超时率
- `helianthus_transaction_failure_rate` - 事务失败率

## 使用方法

### 1. 基本使用

```cpp
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"
#include "Shared/MessageQueue/MessageQueue.h"

using namespace Helianthus::Monitoring;
using namespace Helianthus::MessageQueue;

int main()
{
    MessageQueue MQ;
    MQ.Initialize();
    
    // 创建增强的 Prometheus 导出器
    EnhancedPrometheusExporter EnhancedExporter;
    EnhancedExporter.Start(9109, [&](){ 
        // 返回基础指标
        return CollectBasicMetrics(MQ); 
    });
    
    // 在批处理操作中更新性能统计
    auto StartTime = std::chrono::high_resolution_clock::now();
    // ... 执行批处理操作 ...
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
    
    EnhancedExporter.UpdateBatchPerformance("my_queue", Duration.count(), MessageCount);
    
    return 0;
}
```

### 2. 零拷贝性能监控

```cpp
// 在零拷贝操作中更新性能统计
auto StartTime = std::chrono::high_resolution_clock::now();

const char* Data = "zero-copy-data";
ZeroCopyBuffer ZB{};
MQ.CreateZeroCopyBuffer(Data, strlen(Data), ZB);
MQ.SendMessageZeroCopy("my_queue", ZB);
MQ.ReleaseZeroCopyBuffer(ZB);

auto EndTime = std::chrono::high_resolution_clock::now();
auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);

EnhancedExporter.UpdateZeroCopyPerformance(Duration.count());
```

### 3. 事务性能监控

```cpp
// 在事务操作中更新性能统计
TransactionId TxId = 0;
if (MQ.BeginTransaction(TxId) == QueueResult::SUCCESS) {
    // ... 执行事务操作 ...
    
    auto CommitStartTime = std::chrono::high_resolution_clock::now();
    bool Success = MQ.CommitTransaction(TxId) == QueueResult::SUCCESS;
    auto CommitEndTime = std::chrono::high_resolution_clock::now();
    auto CommitDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(CommitEndTime - CommitStartTime);
    
    EnhancedExporter.UpdateTransactionPerformance(
        Success, !Success, false, false, 
        Success ? CommitDuration.count() : 0, 0
    );
}
```

## 直方图桶配置

增强的 Prometheus 导出器使用预定义的直方图桶，范围从 0.001ms 到 100ms：

```
0.001, 0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1.0, 2.5, 5.0, 7.5, 10.0, 25.0, 50.0, 75.0, 100.0
```

这些桶适用于大多数消息队列操作的性能监控需求。

## Grafana 仪表板

项目提供了完整的 Grafana 仪表板配置文件 `Docs/grafana-dashboard.json`，包含：

1. **批处理性能面板** - 显示批处理耗时和吞吐量
2. **零拷贝性能面板** - 显示零拷贝操作性能
3. **事务性能面板** - 显示事务提交/回滚性能
4. **直方图热力图** - 可视化性能分布
5. **系统健康状态** - 显示关键指标概览

### 导入仪表板

1. 在 Grafana 中创建新的数据源（Prometheus）
2. 导入 `Docs/grafana-dashboard.json` 文件
3. 配置数据源为你的 Prometheus 实例
4. 仪表板将自动显示所有增强指标

## 示例程序

运行增强的 Prometheus 示例：

```bash
# 构建示例
bazel build //Examples:enhanced_prometheus_example

# 运行示例
bazel-bin/Examples/enhanced_prometheus_example
```

示例程序会：
- 启动增强的 Prometheus 导出器在端口 9109
- 模拟批处理、零拷贝和事务操作
- 实时更新性能统计
- 每 10 秒打印统计摘要

访问 `http://localhost:9109/metrics` 查看完整的指标输出。

## 性能考虑

1. **内存使用** - 延迟直方图最多保存 10,000 个样本
2. **线程安全** - 所有统计更新都是线程安全的
3. **实时性** - 指标更新是实时的，无需等待轮询
4. **扩展性** - 支持按队列名称分组统计

## 故障排除

### 常见问题

1. **指标不更新** - 确保调用了相应的 Update 方法
2. **直方图为空** - 检查是否有足够的样本数据
3. **性能影响** - 在高频操作中考虑降低采样率

### 调试技巧

1. 使用 `GetBatchStats()`、`GetZeroCopyStats()`、`GetTransactionStats()` 获取当前统计
2. 调用 `ResetAllStats()` 重置所有统计
3. 检查 Prometheus 指标格式是否正确

## 未来改进

1. **可配置直方图桶** - 支持自定义桶配置
2. **更多分位数** - 添加 P999、P9999 等分位数
3. **聚合统计** - 支持跨队列的聚合统计
4. **告警规则** - 提供预配置的告警规则
