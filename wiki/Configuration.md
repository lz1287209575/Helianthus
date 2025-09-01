# 配置说明

## 📋 概述

Helianthus 提供了丰富的配置选项，可以根据不同的使用场景进行优化。

## ⚙️ 基本配置

### MessageQueueConfig

消息队列系统的基本配置：

```cpp
struct MessageQueueConfig {
    uint32_t MaxQueues;                 // 最大队列数量
    uint32_t DefaultTimeoutMs;          // 默认超时时间
    uint32_t MaxMessageSize;             // 最大消息大小
    bool EnableMetrics;                 // 启用指标收集
    std::string MetricsHost;            // 指标服务主机
    uint16_t MetricsPort;               // 指标服务端口
};
```

### 配置示例

```cpp
MessageQueueConfig Config;
Config.MaxQueues = 100;
Config.DefaultTimeoutMs = 5000;
Config.MaxMessageSize = 1024 * 1024;  // 1MB
Config.EnableMetrics = true;
Config.MetricsHost = "0.0.0.0";
Config.MetricsPort = 9090;

auto Queue = std::make_unique<MessageQueue>();
Queue->Initialize(Config);
```

## 🗂️ 队列配置

### QueueConfig

单个队列的详细配置：

```cpp
struct QueueConfig {
    std::string Name;                    // 队列名称
    PersistenceMode Persistence;        // 持久化模式
    uint32_t MaxSize;                    // 最大队列大小
    uint32_t TimeoutMs;                  // 超时时间
    CompressionConfig Compression;       // 压缩配置
    EncryptionConfig Encryption;        // 加密配置
    bool EnablePriority;                 // 启用优先级
    uint32_t MaxPriority;                // 最大优先级
};
```

### 持久化模式

```cpp
enum class PersistenceMode {
    MEMORY_ONLY,        // 仅内存 - 最快，重启后数据丢失
    FILE_BASED,         // 文件持久化 - 平衡性能和持久性
    DATABASE_BASED      // 数据库持久化 - 最高可靠性
};
```

### 配置示例

```cpp
// 高性能内存队列
QueueConfig HighPerfConfig;
HighPerfConfig.Name = "high_perf_queue";
HighPerfConfig.Persistence = PersistenceMode::MEMORY_ONLY;
HighPerfConfig.MaxSize = 100000;
HighPerfConfig.TimeoutMs = 1000;
HighPerfConfig.EnablePriority = true;
HighPerfConfig.MaxPriority = 10;

// 持久化队列
QueueConfig PersistentConfig;
PersistentConfig.Name = "persistent_queue";
PersistentConfig.Persistence = PersistenceMode::FILE_BASED;
PersistentConfig.MaxSize = 50000;
PersistentConfig.TimeoutMs = 5000;
```

## 🗜️ 压缩配置

### CompressionConfig

数据压缩配置：

```cpp
struct CompressionConfig {
    CompressionAlgorithm Algorithm;     // 压缩算法
    uint8_t Level;                      // 压缩级别 (1-9)
    uint32_t MinSize;                   // 最小压缩大小
    bool EnableAutoCompression;         // 自动压缩
    double CompressionRatioThreshold;   // 压缩率阈值
};
```

### 压缩算法

```cpp
enum class CompressionAlgorithm {
    NONE,           // 不压缩
    GZIP,           // GZIP 压缩
    LZ4,            // LZ4 快速压缩
    ZSTD,           // ZSTD 高压缩比
    SNAPPY          // Snappy 快速压缩
};
```

### 配置示例

```cpp
// GZIP 压缩配置
CompressionConfig GzipConfig;
GzipConfig.Algorithm = CompressionAlgorithm::GZIP;
GzipConfig.Level = 6;                  // 平衡压缩率和速度
GzipConfig.MinSize = 100;               // 最小 100 字节才压缩
GzipConfig.EnableAutoCompression = true;
GzipConfig.CompressionRatioThreshold = 0.8;  // 压缩率低于 80% 不压缩

// 快速压缩配置
CompressionConfig FastConfig;
FastConfig.Algorithm = CompressionAlgorithm::LZ4;
FastConfig.Level = 1;                  // 最快压缩
FastConfig.MinSize = 50;                // 更小的最小大小
FastConfig.EnableAutoCompression = true;
```

## 🔐 加密配置

### EncryptionConfig

数据加密配置：

```cpp
struct EncryptionConfig {
    EncryptionAlgorithm Algorithm;      // 加密算法
    std::string Key;                   // 加密密钥
    std::vector<uint8_t> IV;            // 初始化向量
    bool EnableAutoEncryption;          // 自动加密
    uint32_t MinSize;                   // 最小加密大小
    std::string KeyDerivationFunction;  // 密钥派生函数
};
```

### 加密算法

```cpp
enum class EncryptionAlgorithm {
    NONE,           // 不加密
    AES_128_CBC,    // AES-128-CBC
    AES_256_CBC,    // AES-256-CBC
    AES_128_GCM,    // AES-128-GCM
    AES_256_GCM,    // AES-256-GCM
    CHACHA20_POLY1305 // ChaCha20-Poly1305
};
```

### 配置示例

```cpp
// AES-256-GCM 加密配置
EncryptionConfig AesConfig;
AesConfig.Algorithm = EncryptionAlgorithm::AES_256_GCM;
AesConfig.Key = "MySecretKey123456789012345678901234567890";
AesConfig.EnableAutoEncryption = true;
AesConfig.MinSize = 64;                // 最小 64 字节才加密

// 从环境变量读取密钥
EncryptionConfig EnvConfig;
EnvConfig.Algorithm = EncryptionAlgorithm::AES_256_GCM;
EnvConfig.Key = std::getenv("HELIANTHUS_ENCRYPTION_KEY");
EnvConfig.EnableAutoEncryption = true;
```

## 📊 监控配置

### PrometheusConfig

Prometheus 指标导出配置：

```cpp
struct PrometheusConfig {
    std::string Host;                   // 监听主机
    uint16_t Port;                      // 监听端口
    bool EnableMetrics;                 // 启用指标
    uint32_t UpdateIntervalMs;          // 更新间隔
    std::vector<std::string> Labels;    // 自定义标签
    bool EnableHistograms;              // 启用直方图
    bool EnableSummaries;               // 启用摘要
};
```

### 配置示例

```cpp
PrometheusConfig MetricsConfig;
MetricsConfig.Host = "0.0.0.0";
MetricsConfig.Port = 9090;
MetricsConfig.EnableMetrics = true;
MetricsConfig.UpdateIntervalMs = 1000;  // 每秒更新
MetricsConfig.Labels = {"environment=production", "version=1.0"};
MetricsConfig.EnableHistograms = true;
MetricsConfig.EnableSummaries = true;
```

## 🔄 事务配置

### TransactionConfig

事务处理配置：

```cpp
struct TransactionConfig {
    uint32_t DefaultTimeoutMs;          // 默认超时时间
    uint32_t MaxConcurrentTx;          // 最大并发事务数
    bool EnableAutoRollback;            // 自动回滚
    uint32_t RollbackRetryCount;        // 回滚重试次数
    std::string LogLevel;               // 日志级别
};
```

### 配置示例

```cpp
TransactionConfig TxConfig;
TxConfig.DefaultTimeoutMs = 30000;     // 30 秒超时
TxConfig.MaxConcurrentTx = 1000;       // 最大 1000 个并发事务
TxConfig.EnableAutoRollback = true;
TxConfig.RollbackRetryCount = 3;
TxConfig.LogLevel = "INFO";
```

## 🎯 性能配置

### PerformanceConfig

性能优化配置：

```cpp
struct PerformanceConfig {
    uint32_t ThreadPoolSize;            // 线程池大小
    uint32_t BatchSize;                  // 批处理大小
    bool EnableZeroCopy;                 // 启用零拷贝
    uint32_t MemoryPoolSize;             // 内存池大小
    bool EnableLockFree;                 // 启用无锁队列
    uint32_t CacheLineSize;              // 缓存行大小
};
```

### 配置示例

```cpp
// 高性能配置
PerformanceConfig HighPerfConfig;
HighPerfConfig.ThreadPoolSize = std::thread::hardware_concurrency();
HighPerfConfig.BatchSize = 1000;
HighPerfConfig.EnableZeroCopy = true;
HighPerfConfig.MemoryPoolSize = 1024 * 1024 * 100;  // 100MB
HighPerfConfig.EnableLockFree = true;
HighPerfConfig.CacheLineSize = 64;

// 内存优化配置
PerformanceConfig MemoryConfig;
MemoryConfig.ThreadPoolSize = 4;
MemoryConfig.BatchSize = 100;
MemoryConfig.EnableZeroCopy = false;
MemoryConfig.MemoryPoolSize = 1024 * 1024 * 10;   // 10MB
MemoryConfig.EnableLockFree = false;
```

## 📝 配置文件

### JSON 配置文件

创建 `config.json` 文件：

```json
{
  "messageQueue": {
    "maxQueues": 100,
    "defaultTimeoutMs": 5000,
    "maxMessageSize": 1048576,
    "enableMetrics": true,
    "metricsHost": "0.0.0.0",
    "metricsPort": 9090
  },
  "queues": {
    "highPerf": {
      "name": "high_perf_queue",
      "persistence": "MEMORY_ONLY",
      "maxSize": 100000,
      "timeoutMs": 1000,
      "enablePriority": true,
      "maxPriority": 10
    },
    "persistent": {
      "name": "persistent_queue",
      "persistence": "FILE_BASED",
      "maxSize": 50000,
      "timeoutMs": 5000
    }
  },
  "compression": {
    "algorithm": "GZIP",
    "level": 6,
    "minSize": 100,
    "enableAutoCompression": true,
    "compressionRatioThreshold": 0.8
  },
  "encryption": {
    "algorithm": "AES_256_GCM",
    "key": "MySecretKey123456789012345678901234567890",
    "enableAutoEncryption": true,
    "minSize": 64
  },
  "monitoring": {
    "host": "0.0.0.0",
    "port": 9090,
    "enableMetrics": true,
    "updateIntervalMs": 1000,
    "labels": ["environment=production", "version=1.0"],
    "enableHistograms": true,
    "enableSummaries": true
  }
}
```

### 加载配置文件

```cpp
#include <fstream>
#include <nlohmann/json.hpp>

nlohmann::json LoadConfig(const std::string& FilePath) {
    std::ifstream File(FilePath);
    if (!File.is_open()) {
        throw std::runtime_error("无法打开配置文件: " + FilePath);
    }
    
    nlohmann::json Config;
    File >> Config;
    return Config;
}

// 使用配置
auto Config = LoadConfig("config.json");
auto Queue = std::make_unique<MessageQueue>();
Queue->Initialize(Config["messageQueue"]);
```

## 🔧 环境变量

### 支持的环境变量

```bash
# 基本配置
export HELIANTHUS_MAX_QUEUES=100
export HELIANTHUS_DEFAULT_TIMEOUT_MS=5000
export HELIANTHUS_MAX_MESSAGE_SIZE=1048576

# 监控配置
export HELIANTHUS_METRICS_HOST=0.0.0.0
export HELIANTHUS_METRICS_PORT=9090
export HELIANTHUS_ENABLE_METRICS=true

# 加密配置
export HELIANTHUS_ENCRYPTION_KEY=MySecretKey123456789012345678901234567890
export HELIANTHUS_ENCRYPTION_ALGORITHM=AES_256_GCM

# 性能配置
export HELIANTHUS_THREAD_POOL_SIZE=8
export HELIANTHUS_BATCH_SIZE=1000
export HELIANTHUS_ENABLE_ZERO_COPY=true
```

### 环境变量优先级

1. 命令行参数
2. 配置文件
3. 环境变量
4. 默认值

## 🎯 配置最佳实践

### 开发环境

```cpp
// 开发环境配置
QueueConfig DevConfig;
DevConfig.Persistence = PersistenceMode::MEMORY_ONLY;  // 快速重启
DevConfig.MaxSize = 1000;                              // 小队列
DevConfig.TimeoutMs = 10000;                           // 长超时
DevConfig.Compression.EnableAutoCompression = false;    // 禁用压缩
DevConfig.Encryption.EnableAutoEncryption = false;    // 禁用加密
```

### 测试环境

```cpp
// 测试环境配置
QueueConfig TestConfig;
TestConfig.Persistence = PersistenceMode::FILE_BASED;  // 部分持久化
TestConfig.MaxSize = 10000;                            // 中等队列
TestConfig.TimeoutMs = 5000;                           // 中等超时
TestConfig.Compression.EnableAutoCompression = true;    // 启用压缩
TestConfig.Encryption.EnableAutoEncryption = false;    // 禁用加密
```

### 生产环境

```cpp
// 生产环境配置
QueueConfig ProdConfig;
ProdConfig.Persistence = PersistenceMode::DATABASE_BASED;  // 完全持久化
ProdConfig.MaxSize = 100000;                               // 大队列
ProdConfig.TimeoutMs = 1000;                               // 短超时
ProdConfig.Compression.EnableAutoCompression = true;       // 启用压缩
ProdConfig.Encryption.EnableAutoEncryption = true;        // 启用加密
```

## 🔗 相关链接

- [快速开始](Getting-Started) - 入门指南
- [API 参考](API-Reference) - 完整 API 文档
- [部署指南](Deployment) - 生产环境部署
- [性能优化](Performance) - 性能调优建议

---

**提示**: 根据实际使用场景调整配置参数，建议先在测试环境验证配置效果。
