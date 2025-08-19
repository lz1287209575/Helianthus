# Helianthus 网络与异步框架 TODO（跨平台）

- 完善 IOCP 重叠 I/O（Windows）
  - 使用对象池/智能指针管理 OVERLAPPED 与缓冲区，确保生命周期安全，避免悬空与竞态。
  - 支持取消（CancelIoEx）与关闭套接字时的安全清理；统一回调完成/取消/错误的语义。
  - 为 `AsyncTcpAcceptor` 接入 `AcceptEx` + `SO_UPDATE_ACCEPT_CONTEXT`，并完善本地/对端地址获取（`GetAcceptExSockaddrs`）。
  - 将 WSA 错误码系统化映射到 `NetworkError`，并统一日志格式。

- Proactor/Async API 能力完善（全平台）
  - 为 UDP 提供 Proactor 路径（Windows: `WSARecvFrom/WSASendTo`，POSIX: 使用 Reactor 适配 Proactor），保持与 TCP 一致的接口与语义。
  - `AsyncTcpSocket`/`AsyncUdpSocket` 写路径：处理部分写、写队列、背压（backpressure），基于写就绪事件或完成回调进行续写与排队。
  - 统一取消与超时语义（支持操作级取消 token、超时参数）。
  - 丰富回调错误语义（超时/取消/连接重置/网络不可达等）。

- Reactor 细化（Linux/BSD/macOS）
  - Epoll：支持边沿触发（EPOLLET）与错误/挂起处理（EPOLLERR/EPOLLHUP），优化事件去抖与批量处理。
  - Kqueue：完善读/写事件注册切换（EV_ADD/EV_DELETE）、错误映射、边沿与电平语义校准。
  - 提供统一的注册/修改/删除返回值与错误转换，保证与 `NetworkError` 一致化。

- IoContext 改进
  - 将当前静态任务队列替换为成员任务队列，提供跨线程 `Post` 的唤醒机制：
    - Linux: `eventfd`/自管道
    - BSD: 自管道
    - Windows: 向 IOCP 投递空完成包或 `WakeByAddressSingle` 等
  - 合并驱动节奏：协调 `ProcessCompletions()` 与 `PollOnce()` 的超时与节拍，避免空转。

- 接口与结构统一
  - 统一 `NativeHandle`/`Fd` 类型（`uintptr_t` 已对齐），严禁平台专有类型外泄到头文件公共接口。
  - 统一 `Async*` API：`AsyncReceive`/`AsyncReceiveFrom`/`AsyncSend`/`AsyncSendTo` 的参数与回调签名；提供取消/超时可选参数。
  - 校准 `TcpSocket`/`UdpSocket` 命名与成员风格，与项目 PascalCase 规范保持一致（已完成大部分，持续检查）。

- 测试与 CI
  - 增加跨平台单元测试：
    - `IoContext.Run()` 驱动下的 TCP/UDP 异步回环（echo）测试。
    - 取消与超时测试（确保回调语义与资源释放正确）。
    - Epoll/Kqueue/IOCP 的最小事件用例与错误路径覆盖。
  - Bazel 下为 Windows 平台添加必要的系统库链接（`Ws2_32.lib`、`Mswsock.lib` 等），用 `select()` 切换。

- 性能与可靠性
  - 缓冲区池化、零拷贝路径（如 `sendmsg`/`readv`/`writev` 等）评估与封装。
  - 轮询批量化与回调批处理，减少上下文切换与调用开销。
  - 指标与统计：扩展连接/操作级别的延迟、吞吐、错误统计并提供导出接口。

- 安全与扩展
  - 预留 TLS/DTLS 集成点（OpenSSL/mbedTLS 等），定义加密通道抽象与握手状态机对接。
  - 为后续 RPC（基于本框架）预留调度/超时/重试策略接口。

> 注：以上 TODO 以跨平台优先级排序：先确保语义一致与基础测试通过，再逐步优化 IOCP/epoll/kqueue 的特性与性能。
