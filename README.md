# Helianthus 游戏服务器

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/your-org/helianthus)
[![C++](https://img.shields.io/badge/C%2B%2B-17%2F20-blue)](https://isocpp.org/)
[![Bazel](https://img.shields.io/badge/build-Bazel-green)](https://bazel.build/)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

Helianthus 是一个高性能、可扩展的微服务游戏服务器架构，采用现代C++开发，支持多种脚本语言集成。

## 🌟 项目特色

- **🏗️ 微服务架构**: 模块化设计，支持独立部署和扩展
- **⚡ 高性能**: 自研网络层、消息队列和服务发现系统
- **🔧 多语言脚本**: 支持 Lua、Python、JavaScript、C# 脚本集成
- **🎯 现代C++**: 基于 C++17/20 标准，性能与安全并重
- **🚀 企业级**: 内置负载均衡、健康检查、故障转移等功能
- **🔄 运行时反射**: 类似UE的反射系统，支持热更新和编辑器开发

## 📋 系统要求

- **编译器**: GCC 9+ / Clang 10+ / MSVC 2019+
- **构建系统**: Bazel 6.0+
- **操作系统**: Linux / macOS / Windows
- **内存**: 建议 8GB+
- **磁盘**: 2GB+ (包含依赖)

## 🚀 快速开始

### 1. 克隆项目
```bash
git clone https://github.com/your-org/helianthus.git
cd helianthus
```

### 2. 安装 Bazel
```bash
# Ubuntu/Debian
sudo apt install bazel

# macOS
brew install bazel

# Windows
# 下载并安装 Bazel from https://bazel.build/install
```

### 3. 构建项目
```bash
# 构建所有组件
bazel build //...

# 构建特定组件
bazel build //Shared/Network:network

# 启用脚本支持构建
bazel build //... --define=ENABLE_LUA_SCRIPTING=1
```

### 4. 运行测试
```bash
# 运行所有测试
bazel test //...

# 运行特定测试
bazel test //Tests/Network:all
```

## 📁 项目结构

```
Helianthus/
├── Services/                    # 微服务模块
│   ├── Gateway/                # 网关服务
│   ├── Auth/                   # 认证服务
│   ├── Game/                   # 游戏逻辑服务
│   ├── Player/                 # 玩家管理服务
│   ├── Match/                  # 匹配服务
│   ├── Realtime/              # 实时通信服务
│   └── Monitor/               # 监控服务
├── Shared/                     # 共享组件
│   ├── Network/               # 自定义网络层
│   ├── Message/               # 消息队列系统
│   ├── Discovery/             # 服务发现
│   ├── Protocol/              # 协议定义
│   ├── Reflection/            # 反射系统
│   ├── Scripting/             # 脚本引擎
│   └── Common/                # 通用工具
├── Tools/                      # 开发工具
│   ├── BuildSystem/           # 自定义构建工具
│   └── Monitoring/            # 监控工具
├── ThirdParty/                # 第三方依赖
├── Tests/                     # 单元测试
└── Docs/                      # 文档
```

## 🏗️ 核心架构

### 网络层 (Network)
- **自定义实现**: 高性能 TCP/UDP/WebSocket 支持
- **连接池管理**: 智能连接复用和管理
- **异步IO**: 基于事件驱动的异步网络处理

### 消息队列 (Message)
- **优先级队列**: 支持消息优先级和路由
- **可靠传输**: 消息确认和重传机制
- **热更新支持**: 运行时消息格式变更

### 服务发现 (Discovery)
- **服务注册**: 自动服务注册和注销
- **负载均衡**: 多种负载均衡策略
- **健康检查**: 实时服务健康监控
- **故障转移**: 自动故障检测和转移

### 反射系统 (Reflection)
- **运行时反射**: 类、属性、方法的运行时信息
- **序列化集成**: 自动序列化和反序列化
- **脚本绑定**: 自动生成脚本语言绑定
- **编辑器支持**: 运行时检查和修改

### 脚本引擎 (Scripting)
- **多语言支持**: Lua/Python/JavaScript/C#
- **热更新**: 无需重启的脚本更新
- **沙箱执行**: 安全的脚本执行环境
- **性能优化**: JIT编译和缓存机制

## 🛠️ 开发指南

### 编码规范
- **命名规范**: 
  - 变量: `UPPER_CASE`
  - 函数/类: `PascalCase`
  - 参数: `PascalCase`
  - 目录: `PascalCase`
- **代码格式**: 使用 `clang-format` 自动格式化
- **接口设计**: 优先使用抽象接口，支持依赖注入

### 构建命令
```bash
# 格式化代码
clang-format -i **/*.{cpp,hpp,h}

# 清理构建
bazel clean

# 调试构建
bazel build //... --compilation_mode=dbg

# 发布构建
bazel build //... --compilation_mode=opt
```

### 脚本语言配置
```bash
# 启用 Lua
bazel build //... --define=ENABLE_LUA_SCRIPTING=1

# 启用 Python
bazel build //... --define=ENABLE_PYTHON_SCRIPTING=1

# 启用 JavaScript
bazel build //... --define=ENABLE_JS_SCRIPTING=1

# 启用 C#/.NET
bazel build //... --define=ENABLE_DOTNET_SCRIPTING=1

# 启用多个脚本引擎
bazel build //... \
  --define=ENABLE_LUA_SCRIPTING=1 \
  --define=ENABLE_PYTHON_SCRIPTING=1
```

## 🔧 配置说明

### 网络配置
```cpp
NetworkConfig config;
config.MaxConnections = 10000;
config.BufferSize = 8192;
config.TimeoutMs = 30000;
config.EnableKeepalive = true;
```

### 消息队列配置
```cpp
MessageQueueConfig config;
config.MaxQueueSize = 10000;
config.MaxMessageSize = 1024 * 1024; // 1MB
config.EnablePersistence = true;
config.EnableCompression = true;
```

### 服务发现配置
```cpp
RegistryConfig config;
config.MaxServices = 1000;
config.DefaultTtlMs = 300000; // 5分钟
config.EnableReplication = true;
```

## 📈 开发路线图

### Phase 1: 核心基础架构 ✅
- [x] 项目基础设施和构建系统
- [x] 网络层接口设计
- [x] 消息队列架构
- [x] 服务发现系统

### Phase 1.5: 反射和脚本 🚧
- [ ] 运行时反射系统实现
- [ ] 多语言脚本引擎集成
- [ ] 自动绑定生成器
- [ ] 热更新支持

### Phase 2: 基础服务 📋
- [ ] 网关服务实现
- [ ] 认证服务实现
- [ ] 玩家管理服务
- [ ] 基础游戏逻辑框架

### Phase 3: 高级功能 📋
- [ ] 实时通信优化
- [ ] 游戏逻辑热更新
- [ ] 分布式缓存系统
- [ ] 性能监控和分析

### Phase 4: 生产特性 📋
- [ ] 自定义监控系统
- [ ] 自定义日志系统
- [ ] 容器化和编排
- [ ] 可视化管理界面

## 🤝 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 🙋 支持与联系

- **问题反馈**: [GitHub Issues](https://github.com/your-org/helianthus/issues)
- **功能建议**: [GitHub Discussions](https://github.com/your-org/helianthus/discussions)
- **文档**: [项目文档](https://helianthus-docs.example.com)

## 🌟 致谢

感谢以下开源项目的支持：
- [spdlog](https://github.com/gabime/spdlog) - 高性能日志库
- [Protocol Buffers](https://developers.google.com/protocol-buffers) - 数据序列化
- [Google Test](https://github.com/google/googletest) - 单元测试框架
- [Bazel](https://bazel.build/) - 构建系统

---

⭐ 如果这个项目对你有帮助，请给我们一个 Star！