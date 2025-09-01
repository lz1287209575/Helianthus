# Helianthus Grafana 仪表板设置指南

## 概述

本文档介绍如何设置 Grafana 仪表板来监控 Helianthus 消息队列系统的性能指标。

## 前置条件

1. **Prometheus** - 用于收集 Helianthus 指标
2. **Grafana** - 用于可视化指标
3. **Helianthus** - 运行中的消息队列系统

## 设置步骤

### 1. 启动 Helianthus 增强 Prometheus 导出器

```bash
# 编译并运行增强的 Prometheus 示例
bazel run //Examples:enhanced_prometheus_example

# 或者使用自定义配置启动
./bazel-bin/Examples/enhanced_prometheus_example
```

导出器将在 `http://localhost:9109/metrics` 提供指标。

### 2. 配置 Prometheus

在 `prometheus.yml` 中添加 Helianthus 目标：

```yaml
scrape_configs:
  - job_name: 'helianthus'
    static_configs:
      - targets: ['localhost:9109']
    scrape_interval: 5s
    metrics_path: '/metrics'
```

### 3. 导入 Grafana 仪表板

1. 登录 Grafana
2. 点击左侧菜单的 "+" 号，选择 "Import"
3. 上传 `Docs/grafana_dashboard.json` 文件
4. 选择 Prometheus 数据源
5. 点击 "Import" 完成导入

## 仪表板功能

### 主要面板

1. **事务成功率** - 显示事务处理的成功率百分比
2. **事务计数器** - 显示总事务数、提交数、回滚数等
3. **事务提交耗时** - 显示提交操作的平均值、P50、P95、P99 耗时
4. **批处理耗时** - 显示批处理操作的各种分位数耗时
5. **零拷贝操作耗时** - 显示零拷贝操作的性能指标
6. **直方图** - 显示各种操作的耗时分布

### 关键指标说明

#### 事务指标
- `helianthus_transaction_success_rate` - 事务成功率 (0.0-1.0)
- `helianthus_transaction_commit_duration_avg_ms` - 平均提交耗时
- `helianthus_transaction_commit_duration_p95_ms` - P95 提交耗时

#### 批处理指标
- `helianthus_batch_duration_avg_ms` - 平均批处理耗时
- `helianthus_batch_count_total` - 总批次数
- `helianthus_batch_messages_total` - 总消息数

#### 零拷贝指标
- `helianthus_zero_copy_duration_avg_ms` - 平均零拷贝操作耗时
- `helianthus_zero_copy_operations_total` - 总零拷贝操作数

## 告警配置

### 建议的告警规则

```yaml
groups:
  - name: helianthus_alerts
    rules:
      - alert: HighTransactionFailureRate
        expr: helianthus_transaction_success_rate < 0.95
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Transaction failure rate is high"
          
      - alert: SlowTransactionCommit
        expr: helianthus_transaction_commit_duration_p95_ms > 100
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Transaction commit is slow"
          
      - alert: SlowBatchProcessing
        expr: helianthus_batch_duration_p95_ms > 50
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Batch processing is slow"
```

## 性能调优建议

### 基于指标的调优

1. **事务成功率低**
   - 检查网络连接稳定性
   - 增加事务超时时间
   - 优化事务逻辑

2. **提交耗时高**
   - 检查磁盘 I/O 性能
   - 优化事务大小
   - 考虑使用批处理

3. **批处理耗时高**
   - 调整批处理大小
   - 优化消息序列化
   - 检查系统资源

4. **零拷贝操作慢**
   - 检查内存分配策略
   - 优化缓冲区大小
   - 考虑使用内存映射

## 故障排除

### 常见问题

1. **指标不显示**
   - 检查 Prometheus 是否能访问 Helianthus 指标端点
   - 验证指标名称是否正确
   - 检查时间范围设置

2. **数据延迟**
   - 调整 Prometheus 抓取间隔
   - 检查网络延迟
   - 验证系统时钟同步

3. **面板显示异常**
   - 检查 Prometheus 查询语法
   - 验证指标类型匹配
   - 检查数据源配置

## 扩展配置

### 自定义查询

可以根据需要添加自定义查询：

```promql
# 计算每分钟的事务处理量
rate(helianthus_transaction_total[1m])

# 计算批处理吞吐量
rate(helianthus_batch_messages_total[1m])

# 计算零拷贝操作频率
rate(helianthus_zero_copy_operations_total[1m])
```

### 多实例监控

对于多实例部署，可以使用标签进行区分：

```yaml
# Prometheus 配置
scrape_configs:
  - job_name: 'helianthus'
    static_configs:
      - targets: ['instance1:9109', 'instance2:9109', 'instance3:9109']
    relabel_configs:
      - source_labels: [__address__]
        target_label: instance
```

## 联系支持

如有问题，请参考：
- Helianthus 项目文档
- Prometheus 官方文档
- Grafana 官方文档
