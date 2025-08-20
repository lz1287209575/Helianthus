# Helianthus 网络与异步框架 TODO（跨平台）

- 完善 IOCP 重叠 I/O（Windows）
  - 已完成：
    - 读满/写满语义（读循环直至缓冲填满或出错/对端关闭；写循环直至发送完成）。
    - 取消能力（`CancelIoEx`）与 `AsyncTcpSocket::Close()` 收敛调用路径。
    - 错误映射细化（WSA 错误 → `NetworkError`），完成队列与 Reactor/Proactor 驱动集成。
  - 待办：
    - 使用对象池/智能指针管理 OVERLAPPED 与缓冲区，确保生命周期安全，避免悬空与竞态。
    - 为 `AsyncTcpAcceptor` 接入 `AcceptEx` + `SO_UPDATE_ACCEPT_CONTEXT`，并完善本地/对端地址获取（`GetAcceptExSockaddrs`）。
    - 统一日志格式与错误可观测性（操作级 Trace/统计）。

- Proactor/Async API 能力完善（全平台）
  - 为 UDP 提供 Proactor 路径（Windows: `WSARecvFrom/WSASendTo`，POSIX: 使用 Reactor 适配 Proactor），保持与 TCP 一致的接口与语义。
  - `AsyncTcpSocket`/`AsyncUdpSocket` 写路径：处理部分写、写队列、背压（backpressure），基于写就绪事件或完成回调进行续写与排队。
  - 统一取消与超时语义（支持操作级取消 token、超时参数）。
  - 丰富回调错误语义（超时/取消/连接重置/网络不可达等）。

- Reactor 细化（Linux/BSD/macOS）
  - 已完成：
    - Epoll：支持边沿触发（EPOLLET）与错误/挂起处理（EPOLLERR/EPOLLHUP），优化事件去抖与批量处理。
    - 统一错误映射系统（`ErrorMapping` 类），支持 POSIX 和 Windows 错误码转换。
    - ReactorEpoll 错误处理增强，所有 `epoll_ctl` 和 `epoll_wait` 调用使用统一错误映射。
  - 待办：
    - Kqueue：完善读/写事件注册切换（EV_ADD/EV_DELETE）、错误映射、边沿与电平语义校准。
    - ReactorIocp：为 Windows IOCP 添加统一错误映射，处理 `GetQueuedCompletionStatus` 错误。
    - 提供统一的注册/修改/删除返回值与错误转换，保证与 `NetworkError` 一致化。

- IoContext 改进
  - 已完成：成员任务队列；简易定时器（`PostDelayed`）。
  - 待办：跨线程 `Post` 的唤醒机制：
    - Linux: `eventfd`/自管道
    - BSD: 自管道
    - Windows: 向 IOCP 投递空完成包或 `WakeByAddressSingle` 等
  - 合并驱动节奏：协调 `ProcessCompletions()` 与 `PollOnce()` 的超时与节拍，避免空转。

- 接口与结构统一
  - 统一 `NativeHandle`/`Fd` 类型（`uintptr_t` 已对齐），严禁平台专有类型外泄到头文件公共接口。
  - 统一 `Async*` API：`AsyncReceive`/`AsyncReceiveFrom`/`AsyncSend`/`AsyncSendTo` 的参数与回调签名；提供取消/超时可选参数。
  - 命名规范：`Asio` 目录内临时/局部变量已统一 PascalCase；持续在 `Shared/Network` 其他子目录推进。

- 测试与 CI
  - 已完成：
    - 基础 UDP 回环（echo）集成测试。
    - 长度前缀协议测试（`MessageProtocol`），覆盖半包/粘包处理。
    - 错误映射系统测试（`ErrorMapping`），验证 POSIX 和 Windows 错误码转换。
    - Epoll 边沿触发和跨线程唤醒机制测试。
  - 待办：
    - `IoContext.Run()` 驱动下的 TCP 异步回环（长度前缀，覆盖半包/粘包与读满语义）。
    - 取消与超时测试（`PostDelayed` + `Cancel`，验证回调语义与资源释放）。
    - Kqueue/IOCP 的最小事件用例与错误路径覆盖。
  - Bazel/Windows：为需要的目标统一链接 `Ws2_32.lib`、`Mswsock.lib`（使用 `select()`），并在 MSVC 下补齐 `/utf-8`。

- 性能与可靠性
  - 已完成：
    - 缓冲区池化系统（`BufferPool`、`PooledBuffer`、`BufferPoolManager`），支持多线程安全、自动增长、零初始化等功能。
    - 缓冲区池测试覆盖（8个测试用例），验证基本功能、池增长、非池化缓冲区、零初始化、全局管理器、便利函数、线程安全性和内存效率。
    - 零拷贝路径系统（`ZeroCopyBuffer`、`ZeroCopyReadBuffer`、`ZeroCopyIO`），支持 scatter-gather I/O 操作。
    - 零拷贝缓冲区测试覆盖（10个测试用例），验证基本功能、片段创建、读取缓冲区、操作结果、I/O支持、便利函数、空缓冲区处理、大缓冲区处理、性能统计和移动语义。
    - TCMalloc 内存分配器集成（`TCMallocWrapper`），替换全局 new/delete 操作符，提供高性能内存分配。
    - TCMalloc 测试覆盖（13个测试用例），验证基本初始化、内存分配、重分配、对齐分配、new/delete操作符、C++17对齐分配、内存统计、线程安全、性能比较、内存泄漏检测、配置功能和便利宏。
  - 待办：
    - 轮询批量化与回调批处理，减少上下文切换与调用开销。
    - 指标与统计：扩展连接/操作级别的延迟、吞吐、错误统计并提供导出接口。

- 安全与扩展
  - 预留 TLS/DTLS 集成点（OpenSSL/mbedTLS 等），定义加密通道抽象与握手状态机对接。
  - 为后续 RPC（基于本框架）预留调度/超时/重试策略接口。

- Reactor 错误映射完善
  - Kqueue 错误映射（BSD/macOS）：
    - 为 `ReactorKqueue` 添加统一错误映射，处理 `kqueue`、`kevent` 系统调用错误。
    - 支持 `EV_ADD`、`EV_DELETE`、`EV_MODIFY` 操作的错误处理。
    - 处理 `ENOMEM`、`EINVAL`、`ENOENT` 等 kqueue 特有错误码。
    - 添加 kqueue 错误映射测试用例。
  - IOCP 错误映射（Windows）：
    - 为 `ReactorIocp` 添加统一错误映射，处理 `CreateIoCompletionPort`、`GetQueuedCompletionStatus` 错误。
    - 支持 `ERROR_INVALID_HANDLE`、`ERROR_INVALID_PARAMETER` 等 Windows 特有错误码。
    - 处理 IOCP 完成端口创建和事件处理错误。
    - 添加 IOCP 错误映射测试用例。

- 其他
  - Windows：增加全局 WinSock 初始化（`WSAStartup/WSACleanup`），套接字类在构造/首次使用前确保初始化。
  - 编译：默认 C++20，Windows 目标覆盖 `/utf-8`，清理编码告警。

> 注：以上 TODO 以跨平台优先级排序：先确保语义一致与基础测试通过，再逐步优化 IOCP/epoll/kqueue 的特性与性能。
