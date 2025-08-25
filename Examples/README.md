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

