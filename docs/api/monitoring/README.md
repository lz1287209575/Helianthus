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
