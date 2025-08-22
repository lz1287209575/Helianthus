#pragma once

#ifdef _WIN32

    #include "Shared/Network/Asio/Proactor.h"

    #include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>

namespace Helianthus::Network::Asio
{
class ProactorIocp : public Proactor
{
public:
    ProactorIocp();
    ~ProactorIocp() override;

    // TCP 异步操作
    void AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler) override;
    void AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler) override;
    void AsyncConnect(Fd Handle, const Network::NetworkAddress& Address, ConnectHandler Handler) override;
    void AsyncAccept(Fd ListenHandle, AcceptResultHandler Handler) override;
    
    // UDP 异步操作
    void AsyncReceiveFrom(Fd Handle, char* Buffer, size_t BufferSize, UdpReceiveHandler Handler) override;
    void AsyncSendTo(Fd Handle, const char* Data, size_t Size, const Network::NetworkAddress& Address, UdpSendHandler Handler) override;
    
    // 通用操作
    void ProcessCompletions(int TimeoutMs) override;
    void Cancel(Fd Handle) override;
    
    // IOCP 唤醒机制
    void Wakeup() override;
    void Stop() override;

private:
    HANDLE IocpHandle;
    
    // IOCP 唤醒机制
    static constexpr ULONG_PTR WAKE_KEY = 0xDEADBEEF;
    static constexpr ULONG_PTR STOP_KEY = 0xDEADCAFE;
    
    struct Op
    {
        OVERLAPPED Ov;
        SOCKET Socket;
        char* Buffer;
        size_t BufferSize;
        const char* ConstData;
        size_t DataSize;
        CompletionHandler Handler;
        bool IsWrite;
        size_t Transferred;
        enum class OpType
        {
            Read,
            Write,
            Accept,
            Connect,
            UdpReceiveFrom,
            UdpSendTo
        } Type;
        SOCKET ListenSocket;           // for Accept
        AcceptResultHandler AcceptCb;  // valid when Type==Accept
        sockaddr_in LocalAddr;         // for Accept
        sockaddr_in RemoteAddr;        // for Accept
        Network::NetworkAddress ConnectAddr;  // for Connect
        ConnectHandler ConnectCb;      // valid when Type==Connect
        
        // UDP 操作相关字段
        UdpReceiveHandler UdpReceiveCb;  // valid when Type==UdpReceiveFrom
        UdpSendHandler UdpSendCb;        // valid when Type==UdpSendTo
        Network::NetworkAddress UdpTargetAddr;  // for UdpSendTo
        sockaddr_in UdpSockAddr;        // for UDP operations
    };

    // 已关联到 IOCP 的句柄集合，避免重复关联
    std::unordered_set<Fd> Associated;
    
    // AcceptEx 并发管理
    struct AcceptExManager
    {
        SOCKET ListenSocket;
        AcceptResultHandler Handler;
        std::vector<Op*> PendingAccepts;
        size_t MaxConcurrentAccepts;
        size_t MinConcurrentAccepts;  // 最小并发数
        size_t TargetConcurrentAccepts;  // 目标并发数
        bool IsActive;
        std::chrono::steady_clock::time_point LastAcceptTime;  // 上次接受连接的时间
        size_t AcceptCount;  // 接受的连接数统计
        size_t ErrorCount;   // 错误数统计
        
        AcceptExManager(SOCKET Socket, AcceptResultHandler Cb, size_t MaxConcurrent = 8, size_t MinConcurrent = 2)
            : ListenSocket(Socket), Handler(std::move(Cb)), 
              MaxConcurrentAccepts(MaxConcurrent), MinConcurrentAccepts(MinConcurrent),
              TargetConcurrentAccepts(MaxConcurrent), IsActive(true),
              LastAcceptTime(std::chrono::steady_clock::now()), AcceptCount(0), ErrorCount(0)
        {}
        
        // 动态调整并发数
        void AdjustConcurrency()
        {
            auto Now = std::chrono::steady_clock::now();
            auto TimeSinceLastAccept = std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastAcceptTime).count();
            
            // 如果最近有连接被接受，增加并发数
            if (TimeSinceLastAccept < 100 && AcceptCount > 0)  // 100ms内有连接
            {
                TargetConcurrentAccepts = std::min(TargetConcurrentAccepts + 1, MaxConcurrentAccepts);
            }
            // 如果长时间没有连接，减少并发数
            else if (TimeSinceLastAccept > 1000)  // 1秒内没有连接
            {
                TargetConcurrentAccepts = std::max(TargetConcurrentAccepts - 1, MinConcurrentAccepts);
            }
            
            // 重置统计
            AcceptCount = 0;
            ErrorCount = 0;
        }
        
        // 获取当前应该维持的并发数
        size_t GetCurrentTarget() const
        {
            return std::min(TargetConcurrentAccepts, MaxConcurrentAccepts);
        }
    };
    
    std::unordered_map<Fd, std::unique_ptr<AcceptExManager>> AcceptExManagers;

    void AssociateSocketIfNeeded(SOCKET Socket);
    // AcceptEx pointer cache
    LPFN_ACCEPTEX AcceptExPtr = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrsPtr = nullptr;
    void EnsureAcceptEx(SOCKET ListenSocket);
    
    // ConnectEx pointer cache
    LPFN_CONNECTEX ConnectExPtr = nullptr;
    void EnsureConnectEx(SOCKET Socket);
    
    // AcceptEx 管理方法
    void StartAcceptEx(Fd ListenHandle, AcceptResultHandler Handler, size_t MaxConcurrent = 4);
    void StopAcceptEx(Fd ListenHandle);
    void SubmitAcceptEx(AcceptExManager* Manager);
    void OnAcceptExComplete(Op* AcceptOp, Network::NetworkError Error);
};
}  // namespace Helianthus::Network::Asio

#endif  // _WIN32
