# Helianthus TLS/SSL 安全模块

本模块提供了完整的TLS/SSL加密通信支持，支持多种实现方式和灵活的配置选项。

## 🏗️ 架构概览

```
ITlsChannel (抽象接口)
└── MockTlsChannel (模拟实现，用于测试和演示)
```

## 📁 文件结构

```
Shared/Network/Security/
├── ITlsChannel.h          # TLS通道抽象接口
├── ITlsChannel.cpp        # 工厂函数实现
├── MockTlsChannel.h       # 模拟TLS实现（头文件）
├── MockTlsChannel.cpp     # 模拟TLS实现
├── BUILD.bazel           # Bazel构建配置
└── README.md             # 本文档
```

## 🚀 使用方法

### 基本使用

```cpp
#include "Shared/Network/Security/ITlsChannel.h"

// 创建TLS通道
auto tlsChannel = Helianthus::Network::Security::CreateTlsChannel();

// 配置TLS
TlsConfig config;
config.CertificateFile = "server.crt";
config.PrivateKeyFile = "server.key";
config.CaCertificateFile = "ca.crt";
config.VerifyPeer = true;

// 初始化
auto result = tlsChannel->Initialize(config);
if (result != NetworkError::NONE) {
    // 处理初始化错误
}

// 设置底层Socket
tlsChannel->SetSocketFd(socketFd);

// 异步握手
tlsChannel->AsyncHandshake([](NetworkError error, TlsHandshakeState state) {
    if (error == NetworkError::NONE && state == TlsHandshakeState::CONNECTED) {
        // 握手成功，可以开始加密通信
    }
});
```

### 数据传输

```cpp
// 异步写入加密数据
const char* data = "Hello, TLS!";
tlsChannel->AsyncWrite(data, strlen(data), [](NetworkError error, size_t bytes) {
    if (error == NetworkError::NONE) {
        std::cout << "发送成功: " << bytes << " 字节" << std::endl;
    }
});

// 异步读取加密数据
char buffer[1024];
tlsChannel->AsyncRead(buffer, sizeof(buffer), [&](NetworkError error, size_t bytes) {
    if (error == NetworkError::NONE) {
        std::cout << "接收到: " << std::string(buffer, bytes) << std::endl;
    }
});
```

## ⚙️ 构建配置

### 使用模拟TLS实现（默认）

```bash
# 构建时使用模拟实现（不需要真实的OpenSSL）
bazel build //Examples:tls_example
```

### 使用真实OpenSSL实现

```bash
# 注意：真实OpenSSL实现已暂时移除
# 当前使用模拟TLS实现进行开发和测试
bazel build //Examples:tls_example
```

## 🔧 配置选项

### TlsConfig 结构

```cpp
struct TlsConfig {
    std::string CertificateFile;         // 服务器证书文件路径
    std::string PrivateKeyFile;          // 服务器私钥文件路径
    std::string CaCertificateFile;       // CA证书文件路径
    std::vector<std::string> CipherSuites; // 支持的加密套件
    bool VerifyPeer = true;              // 是否验证对端证书
    bool RequireClientCertificate = false; // 是否要求客户端证书
    uint32_t HandshakeTimeoutMs = 30000; // 握手超时时间（毫秒）
};
```

### 构建时配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| 当前仅支持模拟TLS实现 | 用于开发和测试 | MockTlsChannel |

## 🧪 测试和示例

### 运行TLS示例

```bash
# 运行TLS示例（模拟实现）
bazel run //Examples:tls_example
```

### 单元测试

```bash
# 运行Security模块测试
bazel test //Tests/Security:security_test
```

## 🔒 安全考虑

### 证书管理

1. **生产环境**：使用由受信任CA签发的证书
2. **开发环境**：可以使用自签名证书
3. **测试环境**：可以使用模拟实现

### 加密套件选择

推荐的安全加密套件：
- TLS 1.3: `TLS_AES_256_GCM_SHA384`
- TLS 1.2: `ECDHE-RSA-AES256-GCM-SHA384`

### 版本支持

- **推荐**：TLS 1.3（最新，最安全）
- **支持**：TLS 1.2（广泛兼容）
- **禁用**：TLS 1.1及以下版本（不安全）

## 🐛 故障排除

### 常见错误

1. **证书文件不存在**
   ```
   错误: CERTIFICATE_ERROR
   解决: 检查证书文件路径，确保文件存在且可读
   ```

2. **握手失败**
   ```
   错误: 握手状态为FAILED
   解决: 检查证书有效性，确保客户端和服务器配置匹配
   ```

3. **OpenSSL头文件找不到**
   ```
   错误: Cannot open include file: 'openssl/ssl.h'
   解决: 确保已正确设置OpenSSL submodule
   ```

### 调试技巧

1. **启用详细日志**：在TLS配置中启用调试模式
2. **检查证书**：使用OpenSSL命令行工具验证证书
3. **网络抓包**：使用Wireshark等工具分析TLS握手过程

## 📚 相关文档

- [TLS Submodule设置指南](../../../docs/TLS_SUBMODULE_SETUP.md)
- [网络模块文档](../README.md)
- [示例程序文档](../../../Examples/README.md)

## 🔄 迁移指南

### 从其他TLS库迁移

1. 实现新的ITlsChannel子类
2. 更新工厂函数
3. 添加相应的BUILD配置
4. 测试兼容性

### 性能优化建议

1. **会话复用**：启用TLS会话复用
2. **硬件加速**：利用CPU的AES-NI指令
3. **证书缓存**：缓存已验证的证书
4. **连接池**：复用TLS连接

## 🤝 贡献指南

1. 遵循项目代码规范
2. 添加充分的单元测试
3. 更新相关文档
4. 考虑安全性和性能影响

---

**注意**：本模块设计为生产就绪的TLS/SSL解决方案，但在部署到生产环境之前，请进行充分的安全审计和测试。
