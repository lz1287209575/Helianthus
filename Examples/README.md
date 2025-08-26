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

## 消息队列与DLQ监控示例 dlq_monitor_test

构建与运行：
```
bazel build //Examples:dlq_monitor_test
bazel run //Examples:dlq_monitor_test
```

示例要点：
- 创建标准队列并发送正常/过期消息，触发 DLQ 逻辑
- 启用结构化日志（文件输出），演示请求/会话/用户等上下文字段
- 启用队列指标：队列长度、入/出队速率、处理时延 P50/P95（滑动窗口）
- 定时输出指标到结构化日志文件（默认 `logs/structured.log`）

可配置项（示例中已设置）：
```
queue->SetGlobalConfig("metrics.interval.ms", "2000");
queue->SetGlobalConfig("metrics.window.ms", "60000");
queue->SetGlobalConfig("metrics.latency.capacity", "1024");
```

查看结构化指标日志：
- Bazel 沙箱内运行，文件位于沙箱工作目录的 `logs/structured.log`
- 若直接执行二进制（非 bazel run），则在可执行文件当前工作目录的 `logs/structured.log`
- 建议使用工具查看：
  ```bash
  # 若在非沙箱环境
  tail -f logs/structured.log | jq .
  ```

查询一次指标（代码中示例）：
```cpp
QueueMetrics m;
queue->GetQueueMetrics("test_dlq_monitor_queue", m);
```

