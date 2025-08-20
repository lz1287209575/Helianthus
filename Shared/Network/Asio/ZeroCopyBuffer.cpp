#include "Shared/Network/Asio/ZeroCopyBuffer.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>

namespace Helianthus::Network::Asio
{
    // ZeroCopyBuffer 实现
    void ZeroCopyBuffer::AddFragment(const BufferFragment& Fragment)
    {
        if (Fragment.Data && Fragment.Size > 0) {
            Fragments.push_back(Fragment);
        }
    }

    void ZeroCopyBuffer::AddFragment(const void* Data, size_t Size)
    {
        if (Data && Size > 0) {
            Fragments.emplace_back(Data, Size);
        }
    }

    void ZeroCopyBuffer::AddFragment(const std::string& Str)
    {
        if (!Str.empty()) {
            Fragments.emplace_back(Str.data(), Str.size());
        }
    }

    void ZeroCopyBuffer::AddFragment(const std::vector<uint8_t>& Bytes)
    {
        if (!Bytes.empty()) {
            Fragments.emplace_back(Bytes.data(), Bytes.size());
        }
    }

    size_t ZeroCopyBuffer::GetTotalSize() const
    {
        size_t TotalSize = 0;
        for (const auto& Fragment : Fragments) {
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
        if (Data && Size > 0) {
            Targets.emplace_back(Data, Size);
        }
    }

    void ZeroCopyReadBuffer::AddTarget(std::vector<uint8_t>& Buffer)
    {
        if (!Buffer.empty()) {
            Targets.emplace_back(Buffer.data(), Buffer.size());
        }
    }

    void ZeroCopyReadBuffer::AddTarget(std::string& Buffer)
    {
        if (!Buffer.empty()) {
            Targets.emplace_back(&Buffer[0], Buffer.size());
        }
    }

    size_t ZeroCopyReadBuffer::GetTotalTargetSize() const
    {
        size_t TotalSize = 0;
        for (const auto& Target : Targets) {
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
        if (Buffer.IsEmpty()) {
            return ZeroCopyResult(0, true);
        }

        const auto& Fragments = Buffer.GetFragments();
        std::vector<struct iovec> Iovecs;
        Iovecs.reserve(Fragments.size());

        // 构建 iovec 数组
        for (const auto& Fragment : Fragments) {
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
        
        if (Result >= 0) {
            Stats.TotalOperations++;
            Stats.TotalBytesTransferred += Result;
            Stats.AverageBytesPerOperation = static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
            return ZeroCopyResult(Result, true);
        } else {
            Stats.FailedOperations++;
            return ZeroCopyResult(0, false, errno);
        }
    }

    ZeroCopyResult ZeroCopyIO::RecvMsg(int Socket, ZeroCopyReadBuffer& Buffer, int Flags)
    {
        if (Buffer.IsEmpty()) {
            return ZeroCopyResult(0, true);
        }

        const auto& Targets = Buffer.GetTargets();
        std::vector<struct iovec> Iovecs;
        Iovecs.reserve(Targets.size());

        // 构建 iovec 数组
        for (const auto& Target : Targets) {
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
        
        if (Result >= 0) {
            Stats.TotalOperations++;
            Stats.TotalBytesTransferred += Result;
            Stats.AverageBytesPerOperation = static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
            return ZeroCopyResult(Result, true);
        } else {
            Stats.FailedOperations++;
            return ZeroCopyResult(0, false, errno);
        }
    }

    ZeroCopyResult ZeroCopyIO::WriteV(int Fd, const ZeroCopyBuffer& Buffer)
    {
        if (Buffer.IsEmpty()) {
            return ZeroCopyResult(0, true);
        }

        const auto& Fragments = Buffer.GetFragments();
        std::vector<struct iovec> Iovecs;
        Iovecs.reserve(Fragments.size());

        // 构建 iovec 数组
        for (const auto& Fragment : Fragments) {
            struct iovec Iov;
            Iov.iov_base = const_cast<void*>(Fragment.Data);
            Iov.iov_len = Fragment.Size;
            Iovecs.push_back(Iov);
        }

        // 执行 writev
        ssize_t Result = writev(Fd, Iovecs.data(), Iovecs.size());
        
        if (Result >= 0) {
            Stats.TotalOperations++;
            Stats.TotalBytesTransferred += Result;
            Stats.AverageBytesPerOperation = static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
            return ZeroCopyResult(Result, true);
        } else {
            Stats.FailedOperations++;
            return ZeroCopyResult(0, false, errno);
        }
    }

    ZeroCopyResult ZeroCopyIO::ReadV(int Fd, ZeroCopyReadBuffer& Buffer)
    {
        if (Buffer.IsEmpty()) {
            return ZeroCopyResult(0, true);
        }

        const auto& Targets = Buffer.GetTargets();
        std::vector<struct iovec> Iovecs;
        Iovecs.reserve(Targets.size());

        // 构建 iovec 数组
        for (const auto& Target : Targets) {
            struct iovec Iov;
            Iov.iov_base = Target.first;
            Iov.iov_len = Target.second;
            Iovecs.push_back(Iov);
        }

        // 执行 readv
        ssize_t Result = readv(Fd, Iovecs.data(), Iovecs.size());
        
        if (Result >= 0) {
            Stats.TotalOperations++;
            Stats.TotalBytesTransferred += Result;
            Stats.AverageBytesPerOperation = static_cast<double>(Stats.TotalBytesTransferred) / Stats.TotalOperations;
            return ZeroCopyResult(Result, true);
        } else {
            Stats.FailedOperations++;
            return ZeroCopyResult(0, false, errno);
        }
    }

    bool ZeroCopyIO::IsSupported()
    {
        // 检查系统是否支持 scatter-gather I/O
        // 在现代 Linux 系统中，这些系统调用都是可用的
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
}
