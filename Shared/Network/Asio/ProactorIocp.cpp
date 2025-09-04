#ifdef _WIN32

    #include "Shared/Network/Asio/ProactorIocp.h"

    #include <algorithm>
    #include <chrono>

    #include <mswsock.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>

namespace Helianthus::Network::Asio
{
static Network::NetworkError ConvertWinSockError(int ErrorCode, bool IsWrite)
{
    switch (ErrorCode)
    {
        case WSAETIMEDOUT:
            return Network::NetworkError::TIMEOUT;
        case WSAECONNRESET:
        case WSAECONNABORTED:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAEACCES:
            return Network::NetworkError::PERMISSION_DENIED;
        case WSAENOBUFS:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case WSAEMSGSIZE:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case WSAEINVAL:
        case WSAENOTSOCK:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEOPNOTSUPP:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEWOULDBLOCK:
            return Network::NetworkError::TIMEOUT;
        case WSAEINPROGRESS:
        case WSAEALREADY:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAENOTCONN:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAESHUTDOWN:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAECONNREFUSED:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEHOSTDOWN:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAENETDOWN:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAENETRESET:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAEISCONN:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAETOOMANYREFS:
        case WSAEPROCLIM:
        case WSAEUSERS:
        case WSAEDQUOT:
        case WSAESTALE:
        case WSAEREMOTE:
            return Network::NetworkError::CONNECTION_FAILED;
        default:
            return IsWrite ? Network::NetworkError::SEND_FAILED
                           : Network::NetworkError::RECEIVE_FAILED;
    }
}
ProactorIocp::ProactorIocp()
    : IocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)), Associated()
{
}

ProactorIocp::~ProactorIocp()
{
    if (IocpHandle)
        CloseHandle(IocpHandle);
}

void ProactorIocp::AssociateSocketIfNeeded(SOCKET Socket)
{
    Fd Key = static_cast<Fd>(Socket);
    if (Associated.find(Key) != Associated.end())
    {
        return;
    }
    HANDLE Handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Socket));
    CreateIoCompletionPort(Handle, IocpHandle, static_cast<ULONG_PTR>(Key), 0);
    Associated.insert(Key);
}

void ProactorIocp::EnsureAcceptEx(SOCKET ListenSocket)
{
    if (AcceptExPtr && GetAcceptExSockaddrsPtr)
        return;

    // 获取 AcceptEx 函数指针
    if (!AcceptExPtr)
    {
        GUID GuidAcceptEx = WSAID_ACCEPTEX;
        DWORD Bytes = 0;
        WSAIoctl(ListenSocket,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &GuidAcceptEx,
                 sizeof(GuidAcceptEx),
                 &AcceptExPtr,
                 sizeof(AcceptExPtr),
                 &Bytes,
                 nullptr,
                 nullptr);
    }

    // 获取 GetAcceptExSockaddrs 函数指针
    if (!GetAcceptExSockaddrsPtr)
    {
        GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        DWORD Bytes = 0;
        WSAIoctl(ListenSocket,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &GuidGetAcceptExSockaddrs,
                 sizeof(GuidGetAcceptExSockaddrs),
                 &GetAcceptExSockaddrsPtr,
                 sizeof(GetAcceptExSockaddrsPtr),
                 &Bytes,
                 nullptr,
                 nullptr);
    }
}

void ProactorIocp::EnsureConnectEx(SOCKET Socket)
{
    if (ConnectExPtr)
        return;

    // 获取 ConnectEx 函数指针
    GUID GuidConnectEx = WSAID_CONNECTEX;
    DWORD Bytes = 0;
    WSAIoctl(Socket,
             SIO_GET_EXTENSION_FUNCTION_POINTER,
             &GuidConnectEx,
             sizeof(GuidConnectEx),
             &ConnectExPtr,
             sizeof(ConnectExPtr),
             &Bytes,
             nullptr,
             nullptr);
}

void ProactorIocp::AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler)
{
    SOCKET Socket = static_cast<SOCKET>(Handle);
    AssociateSocketIfNeeded(Socket);
    auto* op = new Op{};
    memset(&op->Ov, 0, sizeof(op->Ov));
    op->Socket = Socket;
    op->Buffer = Buffer;
    op->BufferSize = BufferSize;
    op->ConstData = nullptr;
    op->DataSize = 0;
    op->Handler = std::move(Handler);
    op->IsWrite = false;
    op->Transferred = 0;
    op->Type = Op::OpType::Read;
    // 使用 WSARecv 提交异步读
    WSABUF BufferDescriptor;
    BufferDescriptor.buf = op->Buffer;
    BufferDescriptor.len = static_cast<ULONG>(op->BufferSize);
    DWORD Flags = 0;
    DWORD Bytes = 0;
    int RecvResult = WSARecv(Socket, &BufferDescriptor, 1, &Bytes, &Flags, &op->Ov, nullptr);
    if (RecvResult == SOCKET_ERROR)
    {
        int ErrorCode = WSAGetLastError();
        if (ErrorCode != WSA_IO_PENDING)
        {
            auto Completion = std::move(op->Handler);
            delete op;
            if (Completion)
            {
                Completion(ConvertWinSockError(ErrorCode, false), 0);
            }
        }
    }
}

void ProactorIocp::AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler)
{
    SOCKET Socket = static_cast<SOCKET>(Handle);
    AssociateSocketIfNeeded(Socket);
    auto* op = new Op{};
    memset(&op->Ov, 0, sizeof(op->Ov));
    op->Socket = Socket;
    op->Buffer = nullptr;
    op->BufferSize = 0;
    op->ConstData = Data;
    op->DataSize = Size;
    op->Handler = std::move(Handler);
    op->IsWrite = true;
    op->Transferred = 0;
    op->Type = Op::OpType::Write;
    // 使用 WSASend 提交异步写
    WSABUF BufferDescriptor;
    BufferDescriptor.buf = const_cast<char*>(op->ConstData);
    BufferDescriptor.len = static_cast<ULONG>(op->DataSize);
    DWORD Bytes = 0;
    int SendResult = WSASend(Socket, &BufferDescriptor, 1, &Bytes, 0, &op->Ov, nullptr);
    if (SendResult == SOCKET_ERROR)
    {
        int ErrorCode = WSAGetLastError();
        if (ErrorCode != WSA_IO_PENDING)
        {
            auto Completion = std::move(op->Handler);
            delete op;
            if (Completion)
            {
                Completion(ConvertWinSockError(ErrorCode, true), 0);
            }
        }
    }
}

void ProactorIocp::AsyncConnect(Fd Handle,
                                const Network::NetworkAddress& Address,
                                ConnectHandler Handler)
{
    SOCKET Socket = static_cast<SOCKET>(Handle);
    AssociateSocketIfNeeded(Socket);
    EnsureConnectEx(Socket);

    if (!ConnectExPtr)
    {
        if (Handler)
            Handler(Network::NetworkError::CONNECTION_FAILED);
        return;
    }

    auto* op = new Op{};
    memset(&op->Ov, 0, sizeof(op->Ov));
    op->Socket = Socket;
    op->Buffer = nullptr;
    op->BufferSize = 0;
    op->ConstData = nullptr;
    op->DataSize = 0;
    op->Handler = nullptr;  // Connect 操作不使用 CompletionHandler
    op->IsWrite = false;
    op->Transferred = 0;
    op->Type = Op::OpType::Connect;
    op->ConnectAddr = Address;
    op->ConnectCb = std::move(Handler);

    // 准备目标地址
    sockaddr_in TargetAddr{};
    TargetAddr.sin_family = AF_INET;
    TargetAddr.sin_port = htons(Address.Port);
    inet_pton(AF_INET, Address.Ip.c_str(), &TargetAddr.sin_addr);

    // 使用 ConnectEx 提交异步连接
    DWORD Bytes = 0;
    bool ConnectResult = ConnectExPtr(Socket,
                                      reinterpret_cast<sockaddr*>(&TargetAddr),
                                      sizeof(TargetAddr),
                                      nullptr,  // 不发送数据
                                      0,
                                      &Bytes,
                                      &op->Ov);

    if (!ConnectResult)
    {
        int ErrorCode = WSAGetLastError();
        if (ErrorCode != WSA_IO_PENDING)
        {
            auto Completion = std::move(op->ConnectCb);
            delete op;
            if (Completion)
            {
                Completion(ConvertWinSockError(ErrorCode, false));
            }
        }
    }
}

void ProactorIocp::ProcessCompletions(int TimeoutMs)
{
    DWORD Bytes = 0;
    ULONG_PTR Key = 0;
    LPOVERLAPPED OverlappedPtr = nullptr;
    bool Ok = GetQueuedCompletionStatus(IocpHandle,
                                        &Bytes,
                                        &Key,
                                        &OverlappedPtr,
                                        TimeoutMs >= 0 ? static_cast<DWORD>(TimeoutMs) : INFINITE);
    if (!Ok && OverlappedPtr == nullptr)
    {
        return;  // timeout
    }
    if (OverlappedPtr == nullptr)
    {
        // 处理特殊的完成键（唤醒和停止）
        if (Key == WAKE_KEY)
        {
            // 唤醒完成包，不做任何处理，仅用于唤醒事件循环
            return;
        }
        else if (Key == STOP_KEY)
        {
            // 停止完成包，可以在这里设置停止标志
            return;
        }
        return;  // spurious
    }
    Op* Operation = reinterpret_cast<Op*>(OverlappedPtr);
    Operation->Transferred += Bytes;
    NetworkError err = NetworkError::NONE;
    size_t ToReport = Operation->Transferred;
    bool Done = true;
    if (!Ok)
    {
        int ErrorCode = GetLastError();
        err = ConvertWinSockError(ErrorCode, Operation->IsWrite);
    }
    else if (Operation->Type == Op::OpType::Read && Bytes == 0)
    {
        err = Network::NetworkError::CONNECTION_CLOSED;
    }
    else
    {
        if (Operation->Type == Op::OpType::Write && Operation->Transferred < Operation->DataSize)
        {
            // 续传写操作
            memset(&Operation->Ov, 0, sizeof(Operation->Ov));
            WSABUF BufferDescriptor;
            BufferDescriptor.buf = const_cast<char*>(Operation->ConstData) + Operation->Transferred;
            BufferDescriptor.len = static_cast<ULONG>(Operation->DataSize - Operation->Transferred);
            DWORD More = 0;
            int R =
                WSASend(Operation->Socket, &BufferDescriptor, 1, &More, 0, &Operation->Ov, nullptr);

            if (R == SOCKET_ERROR)
            {
                int ErrorCode = WSAGetLastError();
                if (ErrorCode == WSA_IO_PENDING)
                {
                    // 操作已提交，等待完成
                    Done = false;
                }
                else
                {
                    // 立即错误，停止续传
                    err = ConvertWinSockError(ErrorCode, true);
                    Done = true;
                }
            }
            else if (R == 0)
            {
                // 同步完成，但可能还有数据需要发送
                if (More > 0)
                {
                    // 部分发送，继续续传
                    Operation->Transferred += More;
                    if (Operation->Transferred >= Operation->DataSize)
                    {
                        // 写满所有数据，完成操作
                        Done = true;
                    }
                    else
                    {
                        // 继续续传
                        Done = false;
                    }
                }
                else
                {
                    // 没有发送任何数据，可能是错误情况
                    err = Network::NetworkError::SEND_FAILED;
                    Done = true;
                }
            }
        }
        else if (Operation->Type == Op::OpType::Read &&
                 Operation->Transferred < Operation->BufferSize)
        {
            // 续传读操作
            memset(&Operation->Ov, 0, sizeof(Operation->Ov));
            WSABUF BufferDescriptor;
            BufferDescriptor.buf = Operation->Buffer + Operation->Transferred;
            BufferDescriptor.len =
                static_cast<ULONG>(Operation->BufferSize - Operation->Transferred);
            DWORD Flags = 0;
            DWORD More = 0;
            int R = WSARecv(
                Operation->Socket, &BufferDescriptor, 1, &More, &Flags, &Operation->Ov, nullptr);

            if (R == SOCKET_ERROR)
            {
                int ErrorCode = WSAGetLastError();
                if (ErrorCode == WSA_IO_PENDING)
                {
                    // 操作已提交，等待完成
                    Done = false;
                }
                else
                {
                    // 立即错误，停止续传
                    err = ConvertWinSockError(ErrorCode, false);
                    Done = true;
                }
            }
            else if (R == 0)
            {
                // 同步完成，但可能还有数据需要读取
                if (More > 0)
                {
                    // 部分读取，继续续传
                    Operation->Transferred += More;
                    if (Operation->Transferred >= Operation->BufferSize)
                    {
                        // 读满缓冲区，完成操作
                        Done = true;
                    }
                    else
                    {
                        // 继续续传
                        Done = false;
                    }
                }
                else
                {
                    // 连接关闭
                    err = Network::NetworkError::CONNECTION_CLOSED;
                    Done = true;
                }
            }
        }
        if (Operation->Type == Op::OpType::Accept)
        {
            // 完成 Accept：更新接受上下文
            setsockopt(Operation->Socket,
                       SOL_SOCKET,
                       SO_UPDATE_ACCEPT_CONTEXT,
                       reinterpret_cast<const char*>(&Operation->ListenSocket),
                       sizeof(Operation->ListenSocket));

            // 使用 GetAcceptExSockaddrs 获取本地和远端地址
            sockaddr* LocalSockaddr = nullptr;
            sockaddr* RemoteSockaddr = nullptr;
            int LocalSockaddrLen = 0;
            int RemoteSockaddrLen = 0;

            if (GetAcceptExSockaddrsPtr)
            {
                GetAcceptExSockaddrsPtr(Operation->Buffer,
                                        0,
                                        sizeof(SOCKADDR_IN) + 16,
                                        sizeof(SOCKADDR_IN) + 16,
                                        &LocalSockaddr,
                                        &LocalSockaddrLen,
                                        &RemoteSockaddr,
                                        &RemoteSockaddrLen);

                // 保存地址信息
                if (LocalSockaddr && LocalSockaddrLen >= sizeof(sockaddr_in))
                {
                    Operation->LocalAddr = *reinterpret_cast<sockaddr_in*>(LocalSockaddr);
                }
                if (RemoteSockaddr && RemoteSockaddrLen >= sizeof(sockaddr_in))
                {
                    Operation->RemoteAddr = *reinterpret_cast<sockaddr_in*>(RemoteSockaddr);
                }
            }

            // 使用新的 AcceptEx 完成处理
            OnAcceptExComplete(Operation, err);
            return;
        }
        else if (Operation->Type == Op::OpType::Connect)
        {
            // 完成 Connect：更新连接上下文
            setsockopt(Operation->Socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);

            // 调用连接完成回调
            auto ConnectCb = std::move(Operation->ConnectCb);
            delete Operation;
            if (ConnectCb)
            {
                ConnectCb(err);
            }
            return;
        }
        else if (Operation->Type == Op::OpType::UdpReceiveFrom)
        {
            // 完成 UDP 接收：提取发送方地址
            Network::NetworkAddress FromAddress;
            if (err == Network::NetworkError::NONE)
            {
                char IpStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &Operation->UdpSockAddr.sin_addr, IpStr, INET_ADDRSTRLEN);
                FromAddress =
                    Network::NetworkAddress(IpStr, ntohs(Operation->UdpSockAddr.sin_port));
            }

            // 调用 UDP 接收完成回调
            auto UdpReceiveCb = std::move(Operation->UdpReceiveCb);
            delete Operation;
            if (UdpReceiveCb)
            {
                UdpReceiveCb(err, ToReport, FromAddress);
            }
            return;
        }
        else if (Operation->Type == Op::OpType::UdpSendTo)
        {
            // 完成 UDP 发送：调用发送完成回调
            auto UdpSendCb = std::move(Operation->UdpSendCb);
            delete Operation;
            if (UdpSendCb)
            {
                UdpSendCb(err, ToReport);
            }
            return;
        }
    }
    if (Done)
    {
        auto Handler = std::move(Operation->Handler);
        delete Operation;
        if (Handler)
        {
            Handler(err, ToReport);
        }
    }
}

void ProactorIocp::AsyncAccept(Fd ListenHandle, AcceptResultHandler Handler)
{
    // 使用新的 AcceptEx 管理机制
    StartAcceptEx(ListenHandle, std::move(Handler));
}

void ProactorIocp::Cancel(Fd Handle)
{
    SOCKET Socket = static_cast<SOCKET>(Handle);
    HANDLE CancelHandle = reinterpret_cast<HANDLE>(Socket);

    // 取消指定句柄上的所有挂起 I/O 操作
    CancelIoEx(CancelHandle, nullptr);

    // 如果是监听套接字，也停止相关的 AcceptEx 操作
    auto It = AcceptExManagers.find(Handle);
    if (It != AcceptExManagers.end())
    {
        StopAcceptEx(Handle);
    }
}

void ProactorIocp::Wakeup()
{
    // 投递一个唤醒完成包
    PostQueuedCompletionStatus(IocpHandle, 0, WAKE_KEY, nullptr);
}

void ProactorIocp::Stop()
{
    // 投递一个停止完成包
    PostQueuedCompletionStatus(IocpHandle, 0, STOP_KEY, nullptr);
}

void ProactorIocp::StartAcceptEx(Fd ListenHandle, AcceptResultHandler Handler, size_t MaxConcurrent)
{
    SOCKET ListenSocket = static_cast<SOCKET>(ListenHandle);
    AssociateSocketIfNeeded(ListenSocket);
    EnsureAcceptEx(ListenSocket);

    if (!AcceptExPtr)
    {
        if (Handler)
            Handler(Network::NetworkError::SOCKET_CREATE_FAILED, 0);
        return;
    }

    // 创建 AcceptEx 管理器
    auto Manager =
        std::make_unique<AcceptExManager>(ListenSocket, std::move(Handler), MaxConcurrent);
    AcceptExManagers[ListenHandle] = std::move(Manager);

    // 提交初始的 AcceptEx 操作，创建多个并发接受操作
    auto* ManagerPtr = AcceptExManagers[ListenHandle].get();
    for (size_t i = 0; i < ManagerPtr->GetCurrentTarget(); ++i)
    {
        SubmitAcceptEx(ManagerPtr);
    }
}

void ProactorIocp::StopAcceptEx(Fd ListenHandle)
{
    auto It = AcceptExManagers.find(ListenHandle);
    if (It != AcceptExManagers.end())
    {
        It->second->IsActive = false;

        // 取消所有挂起的 AcceptEx 操作
        for (auto* Op : It->second->PendingAccepts)
        {
            CancelIoEx(reinterpret_cast<HANDLE>(Op->Socket), &Op->Ov);
        }

        // 清理资源
        for (auto* Op : It->second->PendingAccepts)
        {
            delete[] Op->Buffer;
            closesocket(Op->Socket);
            delete Op;
        }

        AcceptExManagers.erase(It);
    }
}

void ProactorIocp::SubmitAcceptEx(AcceptExManager* Manager)
{
    if (!Manager || !Manager->IsActive)
        return;

    // 检查是否达到当前目标并发数
    if (Manager->PendingAccepts.size() >= Manager->GetCurrentTarget())
        return;

    // 创建接受套接字
    SOCKET AcceptSocket =
        ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (AcceptSocket == INVALID_SOCKET)
    {
        if (Manager->Handler)
            Manager->Handler(Network::NetworkError::SOCKET_CREATE_FAILED, 0);
        return;
    }

    AssociateSocketIfNeeded(AcceptSocket);

    // 创建 AcceptEx 操作
    const DWORD BufferLength = (sizeof(SOCKADDR_IN) + 16) * 2;
    char* AddrBuffer = new char[BufferLength];
    auto* AcceptOp = new Op{};
    std::memset(&AcceptOp->Ov, 0, sizeof(AcceptOp->Ov));
    AcceptOp->Socket = AcceptSocket;
    AcceptOp->Buffer = AddrBuffer;
    AcceptOp->BufferSize = BufferLength;
    AcceptOp->ConstData = nullptr;
    AcceptOp->DataSize = 0;
    AcceptOp->IsWrite = false;
    AcceptOp->Transferred = 0;
    AcceptOp->Type = Op::OpType::Accept;
    AcceptOp->ListenSocket = Manager->ListenSocket;
    AcceptOp->AcceptCb = nullptr;  // 使用管理器的 Handler

    // 添加到挂起列表
    Manager->PendingAccepts.push_back(AcceptOp);

    // 提交 AcceptEx
    DWORD Bytes = 0;
    bool R = AcceptExPtr(Manager->ListenSocket,
                         AcceptSocket,
                         AddrBuffer,
                         0,
                         sizeof(SOCKADDR_IN) + 16,
                         sizeof(SOCKADDR_IN) + 16,
                         &Bytes,
                         &AcceptOp->Ov);

    if (!R && WSAGetLastError() != WSA_IO_PENDING)
    {
        // 错误处理
        auto Error = ConvertWinSockError(WSAGetLastError(), false);
        OnAcceptExComplete(AcceptOp, Error);
    }
}

void ProactorIocp::OnAcceptExComplete(Op* AcceptOp, Network::NetworkError Error)
{
    // 查找对应的管理器
    AcceptExManager* Manager = nullptr;
    for (auto& Pair : AcceptExManagers)
    {
        if (Pair.second->ListenSocket == AcceptOp->ListenSocket)
        {
            Manager = Pair.second.get();
            break;
        }
    }

    if (!Manager)
    {
        // 管理器不存在，清理资源
        delete[] AcceptOp->Buffer;
        closesocket(AcceptOp->Socket);
        delete AcceptOp;
        return;
    }

    // 从挂起列表中移除
    auto It = std::find(Manager->PendingAccepts.begin(), Manager->PendingAccepts.end(), AcceptOp);
    if (It != Manager->PendingAccepts.end())
    {
        Manager->PendingAccepts.erase(It);
    }

    if (Error == Network::NetworkError::NONE)
    {
        // 更新统计信息
        Manager->LastAcceptTime = std::chrono::steady_clock::now();
        Manager->AcceptCount++;

        // 成功接受连接
        if (Manager->Handler)
        {
            Manager->Handler(Error, static_cast<Fd>(AcceptOp->Socket));
        }

        // 清理资源
        delete[] AcceptOp->Buffer;
        delete AcceptOp;

        // 动态调整并发数
        Manager->AdjustConcurrency();

        // 提交新的 AcceptEx 操作以维持并发数
        if (Manager->IsActive)
        {
            SubmitAcceptEx(Manager);
        }
    }
    else
    {
        // 更新错误统计
        Manager->ErrorCount++;

        // 错误处理
        if (Manager->Handler)
        {
            Manager->Handler(Error, 0);
        }

        // 清理资源
        delete[] AcceptOp->Buffer;
        closesocket(AcceptOp->Socket);
        delete AcceptOp;

        // 如果是临时错误，重试
        if (Manager->IsActive && (Error == Network::NetworkError::TIMEOUT ||
                                  Error == Network::NetworkError::NETWORK_UNREACHABLE))
        {
            SubmitAcceptEx(Manager);
        }
        // 如果错误率过高，减少并发数
        else if (Manager->ErrorCount > 5)
        {
            Manager->TargetConcurrentAccepts =
                std::max(Manager->TargetConcurrentAccepts - 1, Manager->MinConcurrentAccepts);
            Manager->ErrorCount = 0;  // 重置错误计数
        }
    }
}

// UDP 异步操作实现
void ProactorIocp::AsyncReceiveFrom(Fd Handle,
                                    char* Buffer,
                                    size_t BufferSize,
                                    UdpReceiveHandler Handler)
{
    SOCKET Socket = static_cast<SOCKET>(Handle);
    AssociateSocketIfNeeded(Socket);

    auto* op = new Op{};
    memset(&op->Ov, 0, sizeof(op->Ov));
    op->Socket = Socket;
    op->Buffer = Buffer;
    op->BufferSize = BufferSize;
    op->ConstData = nullptr;
    op->DataSize = 0;
    op->Handler = nullptr;  // UDP 操作使用专门的回调
    op->IsWrite = false;
    op->Transferred = 0;
    op->Type = Op::OpType::UdpReceiveFrom;
    op->UdpReceiveCb = std::move(Handler);

    // 使用 WSARecvFrom 提交异步接收
    WSABUF BufferDescriptor;
    BufferDescriptor.buf = Buffer;
    BufferDescriptor.len = static_cast<ULONG>(BufferSize);
    DWORD Flags = 0;
    DWORD Bytes = 0;
    int SockAddrLen = sizeof(op->UdpSockAddr);

    int RecvResult = WSARecvFrom(Socket,
                                 &BufferDescriptor,
                                 1,
                                 &Bytes,
                                 &Flags,
                                 reinterpret_cast<sockaddr*>(&op->UdpSockAddr),
                                 &SockAddrLen,
                                 &op->Ov,
                                 nullptr);

    if (RecvResult == SOCKET_ERROR)
    {
        int ErrorCode = WSAGetLastError();
        if (ErrorCode != WSA_IO_PENDING)
        {
            auto Completion = std::move(op->UdpReceiveCb);
            delete op;
            if (Completion)
            {
                Completion(ConvertWinSockError(ErrorCode, false), 0, Network::NetworkAddress{});
            }
        }
    }
}

void ProactorIocp::AsyncSendTo(Fd Handle,
                               const char* Data,
                               size_t Size,
                               const Network::NetworkAddress& Address,
                               UdpSendHandler Handler)
{
    SOCKET Socket = static_cast<SOCKET>(Handle);
    AssociateSocketIfNeeded(Socket);

    auto* op = new Op{};
    memset(&op->Ov, 0, sizeof(op->Ov));
    op->Socket = Socket;
    op->Buffer = nullptr;
    op->BufferSize = 0;
    op->ConstData = Data;
    op->DataSize = Size;
    op->Handler = nullptr;  // UDP 操作使用专门的回调
    op->IsWrite = true;
    op->Transferred = 0;
    op->Type = Op::OpType::UdpSendTo;
    op->UdpSendCb = std::move(Handler);
    op->UdpTargetAddr = Address;

    // 准备目标地址
    op->UdpSockAddr.sin_family = AF_INET;
    op->UdpSockAddr.sin_port = htons(Address.Port);
    inet_pton(AF_INET, Address.Ip.c_str(), &op->UdpSockAddr.sin_addr);

    // 使用 WSASendTo 提交异步发送
    WSABUF BufferDescriptor;
    BufferDescriptor.buf = const_cast<char*>(Data);
    BufferDescriptor.len = static_cast<ULONG>(Size);
    DWORD Bytes = 0;

    int SendResult = WSASendTo(Socket,
                               &BufferDescriptor,
                               1,
                               &Bytes,
                               0,
                               reinterpret_cast<sockaddr*>(&op->UdpSockAddr),
                               sizeof(op->UdpSockAddr),
                               &op->Ov,
                               nullptr);

    if (SendResult == SOCKET_ERROR)
    {
        int ErrorCode = WSAGetLastError();
        if (ErrorCode != WSA_IO_PENDING)
        {
            auto Completion = std::move(op->UdpSendCb);
            delete op;
            if (Completion)
            {
                Completion(ConvertWinSockError(ErrorCode, true), 0);
            }
        }
    }
}

}  // namespace Helianthus::Network::Asio

#endif  // _WIN32
