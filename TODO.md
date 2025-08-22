# Helianthus 网络与异步框架 TODO（跨平台）

## 🎯 P0：近期优先事项（已完成核心功能）

### IOCP 唤醒机制（跨线程 Post/Stop 立即生效）
- [x] 在 `ProactorIocp` 中引入 Wake Key，`IoContext::Post/Stop` 调用 `PostQueuedCompletionStatus(IocpHandle, 0, WakeKey, nullptr)`
- [x] `ProcessCompletions` 识别 WakeKey 不做错误处理，仅用于唤醒

### AcceptEx 全流程与持续投递
- [x] 每个挂起 accept 预创建 `WSASocket` 与固定 `AcceptBuffer`（`sizeof(sockaddr_in)*2 + 32`）
- [x] 完成后 `SO_UPDATE_ACCEPT_CONTEXT`，使用 `GetAcceptExSockaddrs` 获取本地/远端地址
- [x] 回调上层并调用 `TcpSocket.Adopt()`
- [x] 维持 1-4 个并发 `AcceptEx`，错误重投递，退出时统一取消

### AsyncRead/AsyncWrite 续传语义
- [x] `WSARecv/WSASend` 完成后根据 `Transferred` 与目标长度继续投递，直至读满/写完或错误
- [x] 统一使用 `ConvertWinSockError` 做错误码映射
- [x] `Cancel(Fd)` 覆盖 Read/Write/Accept，挂起操作被取消应返回一致错误

### AsyncConnect（Windows）
- [x] 使用 `ConnectEx` 实现非阻塞连接，完成后 `SO_UPDATE_CONNECT_CONTEXT`
- [x] 与超时/取消、错误码映射打通

### IoContext 驱动与停止
- [x] Stop/Cancel 时向 IOCP 投递唤醒包，确保 `Run()` 能尽快退出
- [x] 统一 `PostDelayed` 定时触发语义（Windows 基于 IOCP 唤醒）

### 测试与构建（Windows）
- [x] IOCP 路径下的 TCP 长度前缀 Echo（含半包/粘包）、并发、多尺寸
- [x] 取消与超时集成测试
- [x] Bazel 目标统一链接 `Ws2_32.lib`、`Mswsock.lib`，MSVC 使用 `/std:c++20` 与 `/utf-8`

## 🔧 核心功能完善

### 完善 IOCP 重叠 I/O（Windows）
- [x] 读满/写满语义（读循环直至缓冲填满或出错/对端关闭；写循环直至发送完成）
- [x] 取消能力（`CancelIoEx`）与 `AsyncTcpSocket::Close()` 收敛调用路径
- [x] 错误映射细化（WSA 错误 → `NetworkError`），完成队列与 Reactor/Proactor 驱动集成
- [x] 使用对象池/智能指针管理 OVERLAPPED 与缓冲区，确保生命周期安全，避免悬空与竞态
- [x] 为 `AsyncTcpAcceptor` 接入 `AcceptEx` + `SO_UPDATE_ACCEPT_CONTEXT`，并完善本地/对端地址获取（`GetAcceptExSockaddrs`）
- [x] 统一日志格式与错误可观测性（操作级 Trace/统计）

### Proactor/Async API 能力完善（全平台）
- [x] 为 UDP 提供 Proactor 路径（Windows: `WSARecvFrom/WSASendTo`，POSIX: 使用 Reactor 适配 Proactor），保持与 TCP 一致的接口与语义
- [ ] `AsyncTcpSocket`/`AsyncUdpSocket` 写路径：处理部分写、写队列、背压（backpressure），基于写就绪事件或完成回调进行续写与排队
- [ ] 统一取消与超时语义（支持操作级取消 token、超时参数）
- [ ] 丰富回调错误语义（超时/取消/连接重置/网络不可达等）

### Reactor 细化（Linux/BSD/macOS）
- [x] Epoll：支持边沿触发（EPOLLET）与错误/挂起处理（EPOLLERR/EPOLLHUP），优化事件去抖与批量处理
- [x] 统一错误映射系统（`ErrorMapping` 类），支持 POSIX 和 Windows 错误码转换
- [x] ReactorEpoll 错误处理增强，所有 `epoll_ctl` 和 `epoll_wait` 调用使用统一错误映射
- [ ] Kqueue：完善读/写事件注册切换（EV_ADD/EV_DELETE）、错误映射、边沿与电平语义校准
- [ ] ReactorIocp：为 Windows IOCP 添加统一错误映射，处理 `GetQueuedCompletionStatus` 错误
- [ ] 提供统一的注册/修改/删除返回值与错误转换，保证与 `NetworkError` 一致化

### IoContext 改进
- [x] 成员任务队列；简易定时器（`PostDelayed`）
- [x] 跨线程 `Post` 的唤醒机制：
  - [x] Linux: `eventfd`/自管道
  - [x] BSD: 自管道
  - [x] Windows: 向 IOCP 投递空完成包或 `WakeByAddressSingle` 等
- [ ] 合并驱动节奏：协调 `ProcessCompletions()` 与 `PollOnce()` 的超时与节拍，避免空转

### 接口与结构统一
- [x] 统一 `NativeHandle`/`Fd` 类型（`uintptr_t` 已对齐），严禁平台专有类型外泄到头文件公共接口
- [ ] 统一 `Async*` API：`AsyncReceive`/`AsyncReceiveFrom`/`AsyncSend`/`AsyncSendTo` 的参数与回调签名；提供取消/超时可选参数
- [x] 命名规范：`Asio` 目录内临时/局部变量已统一 PascalCase；持续在 `Shared/Network` 其他子目录推进

## 🧪 测试与 CI

### 已完成测试
- [x] 基础 UDP 回环（echo）集成测试
- [x] 长度前缀协议测试（`MessageProtocol`），覆盖半包/粘包处理
- [x] 错误映射系统测试（`ErrorMapping`），验证 POSIX 和 Windows 错误码转换
- [x] Epoll 边沿触发和跨线程唤醒机制测试
- [x] 脚本引擎测试（Lua 集成、热更新功能）
- [x] **IOCP 核心功能测试**：
  - [x] `AsyncReadWriteTest.cpp` - AsyncRead/AsyncWrite 续传语义
  - [x] `AsyncConnectTest.cpp` - ConnectEx 异步连接
  - [x] `IoContextStopTest.cpp` - IoContext 驱动与停止
  - [x] `IocpWakeupTest.cpp` - IOCP 唤醒机制
  - [x] `AcceptExTest.cpp` - AcceptEx 异步接受
  - [x] `CancelTimeoutTest.cpp` - 取消与超时测试
- [x] **UDP Proactor 功能测试**：
  - [x] `UdpProactorTest.cpp` - UDP 异步接收/发送、并发操作、错误处理

### 待办测试
- [ ] `IoContext.Run()` 驱动下的 TCP 异步回环（长度前缀，覆盖半包/粘包与读满语义）
- [ ] Kqueue/IOCP 的最小事件用例与错误路径覆盖
- [ ] 热更新功能集成测试（文件监控、脚本重载、错误处理）

### Bazel/Windows 构建
- [x] 为需要的目标统一链接 `Ws2_32.lib`、`Mswsock.lib`（使用 `select()`），并在 MSVC 下补齐 `/utf-8`

## ⚡ 性能与可靠性

### 已完成功能
- [x] 缓冲区池化系统（`BufferPool`、`PooledBuffer`、`BufferPoolManager`），支持多线程安全、自动增长、零初始化等功能
- [x] 缓冲区池测试覆盖（8个测试用例），验证基本功能、池增长、非池化缓冲区、零初始化、全局管理器、便利函数、线程安全性和内存效率
- [x] 零拷贝路径系统（`ZeroCopyBuffer`、`ZeroCopyReadBuffer`、`ZeroCopyIO`），支持 scatter-gather I/O 操作
- [x] 零拷贝缓冲区测试覆盖（10个测试用例），验证基本功能、片段创建、读取缓冲区、操作结果、I/O支持、便利函数、空缓冲区处理、大缓冲区处理、性能统计和移动语义
- [x] TCMalloc 内存分配器集成（`TCMallocWrapper`），替换全局 new/delete 操作符，提供高性能内存分配
- [x] TCMalloc 测试覆盖（13个测试用例），验证基本初始化、内存分配、重分配、对齐分配、new/delete操作符、C++17对齐分配、内存统计、线程安全、性能比较、内存泄漏检测、配置功能和便利宏

### 待办优化
- [x] 轮询批量化与回调批处理，减少上下文切换与调用开销
- [x] 指标与统计：扩展连接/操作级别的延迟、吞吐、错误统计并提供导出接口

## 🔐 安全与扩展

### 安全功能
- [ ] 预留 TLS/DTLS 集成点（OpenSSL/mbedTLS 等），定义加密通道抽象与握手状态机对接
- [ ] 脚本安全沙箱：限制危险库、提供白名单模块、超时/内存限制

### 扩展功能
- [ ] 为后续 RPC（基于本框架）预留调度/超时/重试策略接口
- [ ] 配置系统集成：从脚本加载配置、热更新钩子

## 📝 脚本系统（已完成基础功能）

### 已完成功能
- [x] 创建 `Shared/Scripting` 目录结构与 `IScriptEngine` 接口
- [x] 实现 `LuaScriptEngine` 类，支持 Lua 5.4 运行时集成
- [x] 配置 `ThirdParty/lua` 作为 git submodule，创建 Bazel 构建目标
- [x] 实装完整的 Lua API 调用：`luaL_newstate`、`luaL_openlibs`、`luaL_loadfile`、`lua_pcall` 等
- [x] 支持宏控制编译：`--define=ENABLE_LUA_SCRIPTING=1` 启用 Lua 功能
- [x] 创建 `Scripts/hello.lua` 示例脚本，包含多种函数示例
- [x] 创建 `Tests/Scripting/ScriptingTest.cpp` 完整测试套件，覆盖初始化、执行字符串、加载文件、调用函数、错误处理等
- [x] 配置 MSVC 和 GCC/Clang 的编译选项，设置正确的依赖关系
- [x] **脚本热更新机制**：`HotReloadManager` 实现文件变更监控、自动重加载
- [x] **热更新集成示例**：`GameServerWithHotReload.cpp` 完整示例
- [x] **热更新文档**：`HOTRELOAD_INTEGRATION.md` 详细集成指南

### 待办功能
- [ ] 性能优化：协程支持、JIT 编译、对象池管理
- [ ] 多语言支持：Python、JavaScript、C# 引擎扩展
- [ ] 反射绑定：与运行时反射系统集成，自动导出 C++ 类到脚本

## 🔧 错误映射完善

### Kqueue 错误映射（BSD/macOS）
- [ ] 为 `ReactorKqueue` 添加统一错误映射，处理 `kqueue`、`kevent` 系统调用错误
- [ ] 支持 `EV_ADD`、`EV_DELETE`、`EV_MODIFY` 操作的错误处理
- [ ] 处理 `ENOMEM`、`EINVAL`、`ENOENT` 等 kqueue 特有错误码
- [ ] 添加 kqueue 错误映射测试用例

### IOCP 错误映射（Windows）
- [ ] 为 `ReactorIocp` 添加统一错误映射，处理 `CreateIoCompletionPort`、`GetQueuedCompletionStatus` 错误
- [ ] 支持 `ERROR_INVALID_HANDLE`、`ERROR_INVALID_PARAMETER` 等 Windows 特有错误码
- [ ] 处理 IOCP 完成端口创建和事件处理错误
- [ ] 添加 IOCP 错误映射测试用例

## 🚀 非网络功能（建议并行推进）

### 结构化日志与可观测性
- [ ] 统一 `Logger` 的字段化输出（连接 ID、操作 ID、错误码、耗时），提供开关与等级
- [ ] 请求级 Trace ID 透传，便于端到端排查

### 指标与统计（Metrics）
- [x] 暴露连接/操作级 QPS、延迟分位（P50/P95/P99）、错误计数（按类型）
- [x] 提供拉取接口或导出到文本（Prometheus 友好格式）

### 配置系统
- [ ] 基于 `json/yaml` 的配置加载、热更新钩子（可选），覆盖端口、缓冲区、并发数、日志等级等
- [ ] 为测试提供最小配置样例与校验

### 基准与性能对比
- [x] 提供 Echo 压测可执行与脚本，生成 CSV/Markdown 报告
- [x] 路径对比：IOCP vs POSIX（epoll）在不同 payload/并发下的吞吐与延迟

### CI 与多平台工况
- [ ] Windows + Linux 双平台流水线
- [ ] 关键测试（Echo、取消/超时、错误映射）并行执行
- [ ] 产物与符号（Release/Debug）归档

## 📊 项目状态总结

### ✅ 已完成主要功能
1. **网络框架基础**：Reactor/Proactor 模式，支持 epoll/kqueue/IOCP
2. **异步 I/O**：TCP/UDP 异步操作，缓冲区管理
3. **内存管理**：TCMalloc 集成，缓冲区池化系统
4. **脚本系统**：Lua 引擎集成，热更新功能
5. **测试覆盖**：基础功能测试套件
6. **Windows IOCP 完整实现**：AcceptEx、ConnectEx、AsyncRead/Write、唤醒机制
7. **UDP Proactor 完整实现**：WSARecvFrom/WSASendTo（Windows）+ Reactor适配（POSIX）
8. **性能监控系统**：完整的性能指标收集和Prometheus导出
9. **批处理优化**：任务批处理和Reactor批处理，自适应批处理大小调整
10. **API统一化**：统一的异步Socket接口，支持取消令牌和超时控制

### 🎯 当前重点
1. **安全功能**：TLS/DTLS集成，脚本沙箱
2. **结构化日志**：字段化日志输出，Trace ID透传
3. **配置系统**：JSON/YAML配置加载，热更新钩子
4. **测试完善**：热更新集成测试，多平台测试覆盖

### 🔮 长期规划
1. **多语言脚本支持**：Python、JavaScript、C#
2. **RPC 框架**：基于网络框架的 RPC 系统
3. **生产就绪**：监控、配置、部署工具
4. **性能基准**：跨平台性能对比和优化

> 注：IOCP核心功能、UDP Proactor、性能监控、批处理优化、API统一化均已完成，项目已具备生产环境的基础能力。当前重点转向安全功能、可观测性和配置系统完善。
