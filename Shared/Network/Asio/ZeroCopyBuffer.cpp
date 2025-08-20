#include "Shared/Network/Asio/ZeroCopyBuffer.h"

#include <algorithm>
#include <cstring>
#include <vector>
#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifndef ssize_t
        #define ssize_t int
    #endif
#else
    #include <errno.h>
    #include <sys/socket.h>
    #include <sys/uio.h>
    #include <unistd.h>
#endif

namespace Helianthus::Network::Asio
{
// ZeroCopyBuffer 实现
void ZeroCopyBuffer::AddFragment(const BufferFragment& Fragment)
{
    if (Fragment.Data && Fragment.Size > 0)
    {
        Fragments.push_back(Fragment);
    }
}

void ZeroCopyBuffer::AddFragment(const void* Data, size_t Size)
{
    if (Data && Size > 0)
    {
        Fragments.emplace_back(Data, Size);
    }
}

void ZeroCopyBuffer::AddFragment(const std::string& Str)
{
    if (!Str.empty())
    {
        Fragments.emplace_back(Str.data(), Str.size());
    }
}

void ZeroCopyBuffer::AddFragment(const std::vector<uint8_t>& Bytes)
{
    if (!Bytes.empty())
    {
        Fragments.emplace_back(Bytes.data(), Bytes.size());
    }
}

size_t ZeroCopyBuffer::GetTotalSize() const
{
    size_t TotalSize = 0;
    for (const auto& Fragment : Fragments)
    {
        TotalSize += Fragment.Size;
    }
    return TotalSize;
}

void ZeroCopyBuffer::Clear()
{
    Fragments.clear();
}

// ZeroCopyReadBuffer 实现
void ZeroCopyReadBuffer::AddTarget(void* Data, size_t Size)
{
    if (Data && Size > 0)
    {
        Targets.emplace_back(Data, Size);
    }
}

void ZeroCopyReadBuffer::AddTarget(std::vector<uint8_t>& Buffer)
{
    if (!Buffer.empty())
    {
        Targets.emplace_back(Buffer.data(), Buffer.size());
    }
}

void ZeroCopyReadBuffer::AddTarget(std::string& Buffer)
{
    if (!Buffer.empty())
    {
        Targets.emplace_back(&Buffer[0], Buffer.size());
    }
}

size_t ZeroCopyReadBuffer::GetTotalTargetSize() const
{
    size_t TotalSize = 0;
    for (const auto& Target : Targets)
    {
        TotalSize += Target.second;
    }
    return TotalSize;
}

void ZeroCopyReadBuffer::Clear()
{
    Targets.clear();
}

// ZeroCopyIO 静态成员初始化
ZeroCopyIO::PerformanceStats ZeroCopyIO::Stats = {};

// ZeroCopyIO 实现
ZeroCopyResult ZeroCopyIO::SendMsg(int Socket, const ZeroCopyBuffer& Buffer, int Flags)
{
#if defined(_WIN32)
    // Windows 不支持 POSIX sendmsg，回退到常规 send 循环
    size_t SentTotal = 0;
    for (const auto& Frag : Buffer.GetFragments())
    {
        if (Frag.Data && Frag.Size)
        {
            int R = ::send(static_cast<SOCKET>(Socket),
                           reinterpret_cast<const char*>(Frag.Data),
                           static_cast<int>(Frag.Size),
                           Flags);
            if (R <= 0)
                return ZeroCopyResult(static_cast<ssize_t>(SentTotal), false, WSAGetLastError());
            SentTotal += static_cast<size_t>(R);
        }
    }
    Stats.TotalOperations++;
    Stats.TotalBytesTransferred += SentTotal;
    Stats.AverageBytesPerOperation =
        static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
    return ZeroCopyResult(static_cast<ssize_t>(SentTotal), true);
#else
    if (Buffer.IsEmpty())
    {
        return ZeroCopyResult(0, true);
    }

    const auto& Fragments = Buffer.GetFragments();
    std::vector<struct iovec> Iovecs;
    Iovecs.reserve(Fragments.size());

    // 构建 iovec 数组
    for (const auto& Fragment : Fragments)
    {
        struct iovec Iov;
        Iov.iov_base = const_cast<void*>(Fragment.Data);
        Iov.iov_len = Fragment.Size;
        Iovecs.push_back(Iov);
    }

    // 构建 msghdr 结构
    struct msghdr Msg;
    std::memset(&Msg, 0, sizeof(Msg));
    Msg.msg_iov = Iovecs.data();
    Msg.msg_iovlen = Iovecs.size();

    // 执行 sendmsg
    ssize_t Result = sendmsg(Socket, &Msg, Flags);

    if (Result >= 0)
    {
        Stats.TotalOperations++;
        Stats.TotalBytesTransferred += Result;
        Stats.AverageBytesPerOperation =
            static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
        return ZeroCopyResult(Result, true);
    }
    else
    {
        Stats.FailedOperations++;
        return ZeroCopyResult(0, false, errno);
    }
#endif
}

ZeroCopyResult ZeroCopyIO::RecvMsg(int Socket, ZeroCopyReadBuffer& Buffer, int Flags)
{
#if defined(_WIN32)
    // Windows 不支持 POSIX recvmsg，回退到常规 recv 循环
    size_t RecvTotal = 0;
    for (auto& Target : Buffer.GetTargets())
    {
        int R = ::recv(static_cast<SOCKET>(Socket),
                       reinterpret_cast<char*>(Target.first),
                       static_cast<int>(Target.second),
                       Flags);
        if (R <= 0)
            return ZeroCopyResult(static_cast<ssize_t>(RecvTotal), false, WSAGetLastError());
        RecvTotal += static_cast<size_t>(R);
    }
    Stats.TotalOperations++;
    Stats.TotalBytesTransferred += RecvTotal;
    Stats.AverageBytesPerOperation =
        static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
    return ZeroCopyResult(static_cast<ssize_t>(RecvTotal), true);
#else
    if (Buffer.IsEmpty())
    {
        return ZeroCopyResult(0, true);
    }

    const auto& Targets = Buffer.GetTargets();
    std::vector<struct iovec> Iovecs;
    Iovecs.reserve(Targets.size());

    // 构建 iovec 数组
    for (const auto& Target : Targets)
    {
        struct iovec Iov;
        Iov.iov_base = Target.first;
        Iov.iov_len = Target.second;
        Iovecs.push_back(Iov);
    }

    // 构建 msghdr 结构
    struct msghdr Msg;
    std::memset(&Msg, 0, sizeof(Msg));
    Msg.msg_iov = Iovecs.data();
    Msg.msg_iovlen = Iovecs.size();

    // 执行 recvmsg
    ssize_t Result = recvmsg(Socket, &Msg, Flags);

    if (Result >= 0)
    {
        Stats.TotalOperations++;
        Stats.TotalBytesTransferred += Result;
        Stats.AverageBytesPerOperation =
            static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
        return ZeroCopyResult(Result, true);
    }
    else
    {
        Stats.FailedOperations++;
        return ZeroCopyResult(0, false, errno);
    }
#endif
}

ZeroCopyResult ZeroCopyIO::WriteV(int Fd, const ZeroCopyBuffer& Buffer)
{
#if defined(_WIN32)
    // Windows 使用 WSASend 代替 writev（此处回退单次发送循环）
    size_t SentTotal = 0;
    for (const auto& Frag : Buffer.GetFragments())
    {
        int R = ::send(static_cast<SOCKET>(Fd),
                       reinterpret_cast<const char*>(Frag.Data),
                       static_cast<int>(Frag.Size),
                       0);
        if (R <= 0)
            return ZeroCopyResult(static_cast<ssize_t>(SentTotal), false, WSAGetLastError());
        SentTotal += static_cast<size_t>(R);
    }
    Stats.TotalOperations++;
    Stats.TotalBytesTransferred += SentTotal;
    Stats.AverageBytesPerOperation =
        static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
    return ZeroCopyResult(static_cast<ssize_t>(SentTotal), true);
#else
    if (Buffer.IsEmpty())
    {
        return ZeroCopyResult(0, true);
    }

    const auto& Fragments = Buffer.GetFragments();
    std::vector<struct iovec> Iovecs;
    Iovecs.reserve(Fragments.size());

    // 构建 iovec 数组
    for (const auto& Fragment : Fragments)
    {
        struct iovec Iov;
        Iov.iov_base = const_cast<void*>(Fragment.Data);
        Iov.iov_len = Fragment.Size;
        Iovecs.push_back(Iov);
    }

    // 执行 writev
    ssize_t Result = writev(Fd, Iovecs.data(), Iovecs.size());

    if (Result >= 0)
    {
        Stats.TotalOperations++;
        Stats.TotalBytesTransferred += Result;
        Stats.AverageBytesPerOperation =
            static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
        return ZeroCopyResult(Result, true);
    }
    else
    {
        Stats.FailedOperations++;
        return ZeroCopyResult(0, false, errno);
    }

#endif
}

ZeroCopyResult ZeroCopyIO::ReadV(int Fd, ZeroCopyReadBuffer& Buffer)
{
#if defined(_WIN32)
    size_t RecvTotal = 0;
    for (auto& Target : Buffer.GetTargets())
    {
        int R = ::recv(static_cast<SOCKET>(Fd),
                       reinterpret_cast<char*>(Target.first),
                       static_cast<int>(Target.second),
                       0);
        if (R <= 0)
            return ZeroCopyResult(static_cast<ssize_t>(RecvTotal), false, WSAGetLastError());
        RecvTotal += static_cast<size_t>(R);
    }
    Stats.TotalOperations++;
    Stats.TotalBytesTransferred += RecvTotal;
    Stats.AverageBytesPerOperation =
        static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
    return ZeroCopyResult(static_cast<ssize_t>(RecvTotal), true);
#else
    if (Buffer.IsEmpty())
    {
        return ZeroCopyResult(0, true);
    }

    const auto& Targets = Buffer.GetTargets();
    std::vector<struct iovec> Iovecs;
    Iovecs.reserve(Targets.size());

    // 构建 iovec 数组
    for (const auto& Target : Targets)
    {
        struct iovec Iov;
        Iov.iov_base = Target.first;
        Iov.iov_len = Target.second;
        Iovecs.push_back(Iov);
    }

    // 执行 readv
    ssize_t Result = readv(Fd, Iovecs.data(), Iovecs.size());

    if (Result >= 0)
    {
        Stats.TotalOperations++;
        Stats.TotalBytesTransferred += Result;
        Stats.AverageBytesPerOperation =
            static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
        return ZeroCopyResult(Result, true);
    }
    else
    {
        Stats.FailedOperations++;
        return ZeroCopyResult(0, false, errno);
    }

#endif
}

bool ZeroCopyIO::IsSupported()
{
    // 检查系统是否支持 scatter-gather I/O
    // 非 Windows：POSIX 下支持；Windows：回退实现
    return true;
}

ZeroCopyIO::PerformanceStats ZeroCopyIO::GetStats()
{
    return Stats;
}

void ZeroCopyIO::ResetStats()
{
    Stats = {};
}
}  // namespace Helianthus::Network::Asio
