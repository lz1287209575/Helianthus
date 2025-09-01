#!/bin/bash

# GitHub Wiki 设置脚本

set -e

echo "🚀 开始设置 Helianthus GitHub Wiki..."

# 检查 Git 是否可用
check_git() {
    if ! command -v git &> /dev/null; then
        echo "❌ Git 未安装，请先安装 Git"
        exit 1
    fi
}

# 创建 Wiki 目录
create_wiki_structure() {
    echo "📁 创建 Wiki 目录结构..."
    
    # 创建 wiki 目录
    mkdir -p wiki
    
    # 创建必要的 Wiki 页面
    local pages=(
        "Home.md"
        "Getting-Started.md"
        "API-Reference.md"
        "Configuration.md"
        "Deployment.md"
        "Monitoring.md"
        "Examples.md"
        "Performance.md"
        "Distributed-Transactions.md"
        "Compression-Encryption.md"
        "Extension-Development.md"
        "Contributing.md"
        "FAQ.md"
        "Changelog.md"
    )
    
    for page in "${pages[@]}"; do
        if [ ! -f "wiki/$page" ]; then
            echo "📝 创建 $page..."
            touch "wiki/$page"
        fi
    done
}

# 生成 Wiki 索引
generate_wiki_index() {
    echo "📋 生成 Wiki 索引..."
    
    cat > wiki/_Sidebar.md << 'EOF'
# Helianthus Wiki

## 🚀 快速开始
- [首页](Home)
- [快速开始](Getting-Started)
- [安装指南](Installation)

## 📚 开发文档
- [API 参考](API-Reference)
- [配置说明](Configuration)
- [示例代码](Examples)

## 🔧 运维指南
- [部署指南](Deployment)
- [监控指南](Monitoring)
- [性能优化](Performance)

## 🔄 高级功能
- [分布式事务](Distributed-Transactions)
- [压缩加密](Compression-Encryption)
- [扩展开发](Extension-Development)

## 🤝 社区
- [贡献指南](Contributing)
- [常见问题](FAQ)
- [更新日志](Changelog)
EOF
}

# 创建部署说明
create_deployment_guide() {
    echo "📖 创建部署说明..."
    
    cat > wiki/Deployment.md << 'EOF'
# 部署指南

## 🚀 生产环境部署

### 系统要求
- **CPU**: 4+ 核心
- **内存**: 8GB+ RAM
- **磁盘**: 100GB+ SSD
- **网络**: 1Gbps+
- **操作系统**: Ubuntu 20.04+ / CentOS 8+

### 1. 环境准备

```bash
# 更新系统
sudo apt update && sudo apt upgrade -y

# 安装依赖
sudo apt install -y build-essential cmake git curl wget

# 安装 Bazel
curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
sudo mv bazel.gpg /etc/apt/trusted.gpg.d/
echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
sudo apt update && sudo apt install bazel
```

### 2. 构建项目

```bash
# 克隆项目
git clone https://github.com/lz1287209575/helianthus.git
cd helianthus

# 生产构建
bazel build //... --config=release --compilation_mode=opt

# 运行测试
bazel test //... --config=release
```

### 3. 配置服务

创建服务配置文件：

```bash
sudo mkdir -p /etc/helianthus
sudo cp config/production.json /etc/helianthus/
sudo chown -R helianthus:helianthus /etc/helianthus/
```

### 4. 启动服务

```bash
# 启动消息队列服务
sudo systemctl start helianthus-mq

# 启动监控服务
sudo systemctl start helianthus-monitor

# 设置开机自启
sudo systemctl enable helianthus-mq
sudo systemctl enable helianthus-monitor
```

### 5. 监控配置

```bash
# 安装 Prometheus
wget https://github.com/prometheus/prometheus/releases/download/v2.45.0/prometheus-2.45.0.linux-amd64.tar.gz
tar xvf prometheus-*.tar.gz
cd prometheus-*

# 配置 Prometheus
cat > prometheus.yml << 'PROMETHEUS_EOF'
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'helianthus'
    static_configs:
      - targets: ['localhost:9090']
PROMETHEUS_EOF

# 启动 Prometheus
./prometheus --config.file=prometheus.yml
```

## 🔧 性能调优

### 系统调优

```bash
# 调整内核参数
echo 'net.core.rmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
echo 'net.core.wmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
echo 'net.ipv4.tcp_rmem = 4096 87380 16777216' | sudo tee -a /etc/sysctl.conf
echo 'net.ipv4.tcp_wmem = 4096 65536 16777216' | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### 应用调优

```json
{
  "performance": {
    "threadPoolSize": 16,
    "batchSize": 1000,
    "enableZeroCopy": true,
    "memoryPoolSize": 1073741824
  },
  "queues": {
    "maxSize": 100000,
    "timeoutMs": 1000
  }
}
```

## 📊 监控告警

### Grafana 仪表板

1. 安装 Grafana
2. 导入 Docs/grafana_dashboard.json
3. 配置 Prometheus 数据源
4. 设置告警规则

### 关键指标

- **队列长度**: helianthus_queue_length
- **消息吞吐量**: helianthus_message_count
- **事务成功率**: helianthus_transaction_success_rate
- **响应时间**: helianthus_batch_duration_ms

## 🔒 安全配置

### 防火墙设置

```bash
# 开放必要端口
sudo ufw allow 9090/tcp  # Prometheus
sudo ufw allow 3000/tcp  # Grafana
sudo ufw allow 8080/tcp  # 应用端口
sudo ufw enable
```

### 加密配置

```json
{
  "encryption": {
    "algorithm": "AES_256_GCM",
    "key": "${HELIANTHUS_ENCRYPTION_KEY}",
    "enableAutoEncryption": true
  }
}
```

## 🚨 故障排查

### 常见问题

1. **服务启动失败**
   ```bash
   sudo journalctl -u helianthus-mq -f
   ```

2. **性能问题**
   ```bash
   # 检查系统资源
   htop
   iostat -x 1
   ```

3. **监控数据异常**
   ```bash
   # 检查 Prometheus 状态
   curl http://localhost:9090/api/v1/status/config
   ```

## 📈 扩展部署

### 集群部署

```bash
# 配置负载均衡
sudo apt install nginx
sudo cp config/nginx.conf /etc/nginx/sites-available/helianthus
sudo ln -s /etc/nginx/sites-available/helianthus /etc/nginx/sites-enabled/
sudo systemctl restart nginx
```

### 容器化部署

```dockerfile
FROM ubuntu:20.04

RUN apt update && apt install -y bazel
COPY . /app
WORKDIR /app

RUN bazel build //... --config=release

EXPOSE 8080 9090
CMD ["./bazel-bin/Examples/helianthus_server"]
```

---

**提示**: 生产环境部署前请务必在测试环境充分验证。
EOF
}

# 创建监控指南
create_monitoring_guide() {
    echo "📊 创建监控指南..."
    
    cat > wiki/Monitoring.md << 'EOF'
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
EOF
}

# 主函数
main() {
    check_git
    create_wiki_structure
    generate_wiki_index
    create_deployment_guide
    create_monitoring_guide
    
    echo "🎉 GitHub Wiki 设置完成！"
    echo ""
    echo "📖 下一步操作："
    echo "1. 将 wiki/ 目录推送到 GitHub Wiki 仓库"
    echo "2. 在 GitHub 项目页面启用 Wiki"
    echo "3. 编辑 Wiki 页面内容"
    echo ""
    echo "📝 推送命令："
    echo "git clone https://github.com/lz1287209575/helianthus.wiki.git"
    echo "cp -r wiki/* helianthus.wiki/"
    echo "cd helianthus.wiki"
    echo "git add ."
    echo "git commit -m 'Add Helianthus Wiki pages'"
    echo "git push origin main"
}

main "$@"
