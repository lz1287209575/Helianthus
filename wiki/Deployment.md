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
