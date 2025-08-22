## Helianthus 性能监控指标（Prometheus）

本框架内置性能监控并提供 Prometheus 友好导出接口。核心位于 `Shared/Network/Asio/PerformanceMetrics.*`。

### 导出方式
- 代码内导出：`PERFORMANCE_MONITOR().ExportPrometheusMetrics()` 返回文本格式，可挂到 HTTP Handler 暴露给 Prometheus。
- 示例参考：`Examples/performance_monitoring_example`（构建后运行可在控制台看到前若干行导出结果）。

### 指标命名与单位
以下为当前导出的关键指标（部分）：

- 连接级（labels: connection_id, remote_address）
  - `connection_total_operations`: 累计操作次数（count）
  - `connection_successful_operations`: 成功操作次数（count）
  - `connection_failed_operations`: 失败操作次数（count）
  - `connection_success_rate`: 成功率（ratio 0~1）
  - `connection_avg_latency_ms`: 平均延迟（ms）
  - `connection_min_latency_ms`: 最小延迟（ms）
  - `connection_max_latency_ms`: 最大延迟（ms）
  - `connection_p50_latency_ms`/`connection_p95_latency_ms`/`connection_p99_latency_ms`: 分位延迟（ms）
  - `connection_throughput_ops_per_sec`: 吞吐（ops/sec）
  - `connection_total_bytes_processed`: 处理字节数（bytes）
  - `connection_total_messages_processed`: 处理消息数（count）
  - 错误分类：`connection_network_errors`/`timeout_errors`/`protocol_errors`/`authentication_errors`/`authorization_errors`/`resource_errors`/`system_errors`/`unknown_errors`
  - 连接池（labels 继承 + pool_type=connection）：`pool_total_size`/`pool_active_connections`/`pool_idle_connections`/`pool_max_connections`/`pool_utilization`/`pool_avg_wait_time_ms`/`pool_exhaustion_count`

- 操作级（labels: operation_id, operation_type, protocol）
  - 与连接级类似：`operation_total_operations`、`operation_success_rate`、`operation_avg_latency_ms`、分位延迟、吞吐与错误分类等
  - 额外：`operation_partial_operations`/`operation_retry_count`/`operation_buffer_overflows`

- 系统级（无额外标签）
  - 连接统计：`system_active_connections`/`system_total_connections`/`system_failed_connections`
  - 资源统计：`system_memory_usage_bytes`/`system_peak_memory_usage_bytes`/`system_thread_count`/`system_cpu_usage_percent`/`system_file_descriptor_count`/`system_buffer_pool_usage`/`system_buffer_pool_capacity`/`system_buffer_pool_utilization`
  - 事件循环：`system_event_loop_iterations`/`system_events_processed`/`system_idle_time_ms`
  - 批处理：`system_batch_processing_count`/`system_average_batch_size`/`system_max_batch_size`

### 使用建议
- 以 HTTP 路由 `/metrics` 暴露 `ExportPrometheusMetrics()` 的返回值。
- 指标抓取周期建议 1~5s；高频场景可 <1s，但应评估开销。
- 对连接/操作 ID 建议使用稳定且可追踪的标识（如连接UUID、请求ID）。

### 示例（片段）
```
# HELP helianthus_network_metrics Helianthus Network Framework Metrics
# TYPE helianthus_network_metrics counter
# Connection Metrics
connection_total_operations{connection_id="db_connection",remote_address="127.0.0.1:3306"} 1000
connection_success_rate{connection_id="db_connection",remote_address="127.0.0.1:3306"} 0.978
connection_avg_latency_ms{connection_id="db_connection",remote_address="127.0.0.1:3306"} 1.49
...
```

### 常见问题
- 分位延迟基于滑动样本（默认1万样本窗口），在流量极低时可能偏差。
- 吞吐依赖 `LastResetTime` 时间窗口，极短时间段内可能为 0。
- 若需直方图类型，可在 `PrometheusExporter::FormatHistogram` 基础上按桶导出，自行扩展。
