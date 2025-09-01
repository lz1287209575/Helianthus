# 监控指南

## 📊 监控架构

Helianthus 采用 Prometheus + Grafana 的监控架构：

```
Helianthus App → Prometheus → Grafana → 告警
```

## 🔧 指标配置

### 启动监控

```cpp
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"

int main() {
    auto Exporter = std::make_unique<Helianthus::Monitoring::EnhancedPrometheusExporter>();
    Exporter->Start("0.0.0.0", 9090);
    
    // 您的应用代码...
    
    return 0;
}
```

### 关键指标

#### 队列指标
- `helianthus_queue_length` - 队列长度
- `helianthus_message_count` - 消息计数
- `helianthus_queue_capacity` - 队列容量

#### 性能指标
- `helianthus_batch_duration_ms` - 批处理耗时
- `helianthus_zero_copy_duration_ms` - 零拷贝耗时
- `helianthus_message_throughput` - 消息吞吐量

#### 事务指标
- `helianthus_transaction_commit_duration_ms` - 事务提交耗时
- `helianthus_transaction_success_rate` - 事务成功率
- `helianthus_transaction_rollback_rate` - 事务回滚率

## 📈 Grafana 仪表板

### 导入仪表板

1. 启动 Grafana
2. 创建 Prometheus 数据源
3. 导入 `Docs/grafana_dashboard.json`

### 仪表板面板

#### 事务监控
- 事务成功率趋势
- 平均提交时间
- 事务分布统计

#### 性能监控
- 批处理性能
- 零拷贝操作统计
- 消息吞吐量

#### 队列状态
- 队列长度监控
- 队列容量使用率
- 消息积压情况

## 🚨 告警配置

### Prometheus 告警规则

```yaml
groups:
  - name: helianthus
    rules:
      - alert: HighTransactionFailureRate
        expr: helianthus_transaction_success_rate < 0.95
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "事务失败率过高"
          description: "事务成功率低于 95%"

      - alert: QueueFull
        expr: helianthus_queue_length / helianthus_queue_capacity > 0.9
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "队列即将满"
          description: "队列使用率超过 90%"

      - alert: HighLatency
        expr: helianthus_batch_duration_ms > 100
        for: 3m
        labels:
          severity: warning
        annotations:
          summary: "批处理延迟过高"
          description: "批处理时间超过 100ms"
```

### 告警通知

配置 Slack 通知：

```yaml
receivers:
  - name: 'slack-notifications'
    slack_configs:
      - channel: '#helianthus-alerts'
        send_resolved: true
        title: '{{ template "slack.title" . }}'
        text: '{{ template "slack.text" . }}'
```

## 🔍 日志监控

### 日志配置

```cpp
// 配置结构化日志
LogConfig Config;
Config.Level = LogLevel::INFO;
Config.Format = LogFormat::JSON;
Config.Output = LogOutput::FILE;
Config.FilePath = "/var/log/helianthus/app.log";
```

### 日志分析

使用 ELK Stack 分析日志：

```bash
# 安装 Elasticsearch
docker run -d --name elasticsearch -p 9200:9200 elasticsearch:7.17.0

# 安装 Logstash
docker run -d --name logstash -p 5044:5044 logstash:7.17.0

# 安装 Kibana
docker run -d --name kibana -p 5601:5601 kibana:7.17.0
```

## 📊 性能分析

### 性能基准测试

```bash
# 运行性能测试
bazel test //Tests/Message:performance_benchmark_test --test_output=all

# 生成性能报告
bazel run //Tools:performance_report_generator
```

### 性能调优

#### 内存优化
- 调整内存池大小
- 优化对象生命周期
- 减少内存拷贝

#### CPU 优化
- 使用多线程处理
- 优化批处理大小
- 启用零拷贝操作

#### 网络优化
- 调整缓冲区大小
- 优化网络参数
- 使用连接池

## 🔧 故障排查

### 常见问题

1. **指标不更新**
   ```bash
   # 检查 Prometheus 配置
   curl http://localhost:9090/api/v1/status/config
   
   # 检查指标端点
   curl http://localhost:9090/metrics
   ```

2. **Grafana 无法连接**
   ```bash
   # 检查数据源配置
   # 验证网络连接
   # 查看 Grafana 日志
   ```

3. **告警不触发**
   ```bash
   # 检查告警规则语法
   # 验证指标表达式
   # 查看 Alertmanager 状态
   ```

### 调试工具

```bash
# 实时监控指标
watch -n 1 'curl -s http://localhost:9090/metrics | grep helianthus'

# 查看系统资源
htop
iostat -x 1
netstat -i
```

## 📚 最佳实践

### 监控策略

1. **分层监控**: 系统层、应用层、业务层
2. **实时监控**: 关键指标实时告警
3. **趋势分析**: 历史数据趋势分析
4. **容量规划**: 基于监控数据规划容量

### 告警策略

1. **分级告警**: 不同严重程度不同处理
2. **避免告警风暴**: 合理设置告警间隔
3. **告警收敛**: 相关告警合并处理
4. **自动恢复**: 简单问题自动处理

---

**提示**: 监控是运维的重要基础，建议在生产环境部署前充分测试监控系统。
