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
 - 分片/副本：示例中配置了 2 个分片、每分片 2 个副本（Leader/Follower），心跳线程会模拟随机健康波动；程序末尾打印每分片状态（`leader` 与 `healthy_followers`）

可配置项（示例中已设置）：
```
Queue->SetGlobalConfig("metrics.interval.ms", "2000");
Queue->SetGlobalConfig("metrics.window.ms", "60000");
Queue->SetGlobalConfig("metrics.latency.capacity", "1024");
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
Queue->GetQueueMetrics("test_dlq_monitor_queue", m);
```

查看分片状态（代码中示例）：
```cpp
std::vector<ShardInfo> Shards;
Queue->GetClusterShardStatuses(Shards);
for (const auto& s : Shards) {
  // 遍历 s.Replicas 获取 leader/healthy followers
}
```

Prometheus 接入建议：
- 方式一：侧车/外部进程解析 `logs/structured.log`，转为 Prometheus 指标（适合快速落地）
  - 使用 Filebeat/Fluent Bit 收集结构化日志到中间层，再由转换器暴露 `/metrics`
- 方式二：在服务内提供 `/metrics` HTTP 路由，导出核心指标（推荐）
  - 示例（伪代码）：
  ```cpp
  // 假设集成了轻量HTTP服务
  app.Get("/metrics", [&](Request&, Response& rsp){
      std::vector<QueueMetrics> metrics;
      Queue->GetAllQueueMetrics(metrics);
      std::ostringstream out;
      out << "# HELP helianthus_queue_pending 当前队列长度" << '\n'
          << "# TYPE helianthus_queue_pending gauge" << '\n';
      for (const auto& m : metrics) {
          out << "helianthus_queue_pending{queue=\"" << m.QueueName << "\"} "
              << m.PendingMessages << "\n";
      }
      // 按需追加 total/processed/dead_letter/enq_rate/deq_rate/p50/p95 等
      rsp.set_body(out.str());
  });
  ```
  - Prometheus 抓取配置示例：
  ```yaml
  scrape_configs:
    - job_name: helianthus-mq
      static_configs:
        - targets: ["127.0.0.1:8080"]
  ```

