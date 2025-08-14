# Helianthus 单元测试套件

本文档描述了 Helianthus 项目的单元测试结构和运行方法。

## 测试结构

### 目录结构
```
Tests/
├── Common/           # 通用模块测试
│   ├── BUILD.bazel
│   └── CommonTest.cpp
├── Network/          # 网络模块测试
│   ├── BUILD.bazel
│   ├── NetworkManagerTest.cpp
│   ├── TcpSocketTest.cpp
│   └── TcpSocketUnitTest.cpp
├── Message/          # 消息模块测试
│   ├── BUILD.bazel
│   ├── MessageTest.cpp
│   ├── MessageQueueTest.cpp
│   └── MessageComprehensiveTest.cpp
├── Discovery/        # 服务发现模块测试
│   ├── BUILD.bazel
│   └── ServiceRegistryTest.cpp
└── README.md         # 本文档
```

## 测试模块

### 1. Common 模块测试 (`Tests/Common/`)
- **文件**: `CommonTest.cpp`
- **目标**: `//Tests/Common:common_test`
- **测试内容**:
  - 类型别名和常量
  - 结果码枚举
  - 日志级别枚举
  - 线程池配置
  - 内存池配置
  - 服务信息结构
  - 时间戳操作
  - 类型大小验证
  - 线程安全性

### 2. Network 模块测试 (`Tests/Network/`)

#### NetworkManager 测试
- **文件**: `NetworkManagerTest.cpp`
- **目标**: `//Tests/Network:network_manager_test`
- **测试内容**:
  - 初始化和配置管理
  - 连接管理
  - 服务器操作
  - 消息处理器回调
  - 消息发送操作
  - 统计和监控
  - 连接分组
  - 地址验证
  - 工具方法
  - 关闭行为

#### TcpSocket 单元测试
- **文件**: `TcpSocketUnitTest.cpp`
- **目标**: `//Tests/Network:tcp_socket_unit_test`
- **测试内容**:
  - 构造函数和初始化
  - 连接 ID 唯一性
  - 本地和远程地址
  - 连接统计
  - 套接字选项设置和获取
  - 阻塞模式设置
  - 回调注册
  - Ping 操作
  - 统计重置
  - 断开连接操作
  - 发送和接收操作
  - 绑定和监听操作
  - 异步接收
  - 移动语义
  - 线程安全性

#### TcpSocket 集成测试
- **文件**: `TcpSocketTest.cpp`
- **目标**: `//Tests/Network:tcp_socket_test` (可执行文件)
- **测试内容**:
  - 端到端服务器-客户端通信
  - 实际网络连接测试

### 3. Message 模块测试 (`Tests/Message/`)

#### Message 基础测试
- **文件**: `MessageTest.cpp`
- **目标**: `//Tests/Message:message_test`
- **测试内容**:
  - 构造函数初始化
  - 负载设置和获取
  - 序列化和反序列化
  - 校验和验证
  - 消息验证
  - 消息属性

#### Message 全面测试
- **文件**: `MessageComprehensiveTest.cpp`
- **目标**: `//Tests/Message:message_comprehensive_test`
- **测试内容**:
  - 默认构造函数
  - 参数化构造函数
  - 带负载的构造函数
  - 二进制负载构造函数
  - 拷贝构造函数
  - 移动构造函数
  - 拷贝赋值
  - 移动赋值
  - 负载设置和获取
  - JSON 负载操作
  - 消息属性
  - 序列化和反序列化
  - 原始数据反序列化
  - 校验和验证
  - 消息验证
  - 校验和计算
  - 总大小计算
  - 重置操作
  - 克隆操作
  - 字符串表示
  - 大负载处理
  - 空负载处理
  - 包含空字节的二进制负载

#### MessageQueue 测试
- **文件**: `MessageQueueTest.cpp`
- **目标**: `//Tests/Message:message_queue_test`
- **测试内容**:
  - 消息队列操作
  - 优先级队列
  - 消息路由

### 4. Discovery 模块测试 (`Tests/Discovery/`)

#### ServiceRegistry 测试
- **文件**: `ServiceRegistryTest.cpp`
- **目标**: `//Tests/Discovery:service_registry_test`
- **测试内容**:
  - 服务注册和注销
  - 服务发现
  - 健康检查
  - 负载均衡
  - 服务分组

## 运行测试

### 运行所有测试
```bash
bazel test //Tests/... --test_output=all
```

### 运行特定模块测试
```bash
# Network 模块
bazel test //Tests/Network/... --test_output=all

# Message 模块
bazel test //Tests/Message/... --test_output=all

# Common 模块
bazel test //Tests/Common/... --test_output=all

# Discovery 模块
bazel test //Tests/Discovery/... --test_output=all
```

### 运行特定测试
```bash
# NetworkManager 测试
bazel test //Tests/Network:network_manager_test --test_output=all

# TcpSocket 单元测试
bazel test //Tests/Network:tcp_socket_unit_test --test_output=all

# Message 全面测试
bazel test //Tests/Message:message_comprehensive_test --test_output=all
```

### 运行集成测试
```bash
# TcpSocket 集成测试
bazel run //Tests/Network:tcp_socket_test
```

## 测试覆盖率

### 当前覆盖率状态
- ✅ **Network 模块**: 基本功能测试完成
- ✅ **Message 模块**: 全面功能测试完成
- ⚠️ **Common 模块**: 部分测试完成（Logger 测试因依赖问题暂缓）
- ⚠️ **Discovery 模块**: 部分测试完成（编译问题待修复）

### 测试质量
- **单元测试**: 覆盖所有公共接口
- **边界条件测试**: 包含错误情况和边界值
- **线程安全测试**: 验证多线程环境下的正确性
- **内存安全测试**: 验证移动语义和资源管理
- **集成测试**: 验证模块间的协作

## 已知问题

### 1. 依赖问题
- **spdlog**: 由于 Bazel 8 的 WORKSPACE 限制，Logger 相关测试暂时禁用
- **Google Test**: 已迁移到 bzlmod，但版本警告存在

### 2. 编译问题
- **Discovery 模块**: 存在命名冲突问题，已部分修复
- **类型定义**: 某些类型定义需要进一步完善

### 3. 测试限制
- **网络测试**: 某些测试需要实际网络环境
- **异步测试**: 时间相关的测试可能需要调整超时时间

## 未来改进

### 1. 测试增强
- [ ] 添加性能测试
- [ ] 添加压力测试
- [ ] 添加内存泄漏测试
- [ ] 添加代码覆盖率报告

### 2. 测试工具
- [ ] 添加测试数据生成器
- [ ] 添加模拟对象框架
- [ ] 添加测试夹具工厂

### 3. 持续集成
- [ ] 配置 CI/CD 流水线
- [ ] 添加自动化测试报告
- [ ] 集成代码质量检查

## 贡献指南

### 添加新测试
1. 在相应模块目录下创建测试文件
2. 更新 BUILD.bazel 文件
3. 遵循现有的测试命名约定
4. 添加适当的测试文档

### 测试命名约定
- 测试类: `{ClassName}Test`
- 测试方法: `{MethodName}_{Scenario}_{ExpectedResult}`
- 文件命名: `{ClassName}Test.cpp`

### 测试最佳实践
- 每个测试应该独立且可重复
- 使用描述性的测试名称
- 包含正面和负面测试用例
- 验证边界条件和错误情况
- 使用适当的断言和错误消息
