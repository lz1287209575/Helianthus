#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Helianthus::Network::Asio
{
// 零拷贝缓冲区片段
struct BufferFragment
{
    const void* Data = nullptr;
    size_t Size = 0;

    BufferFragment() = default;
    BufferFragment(const void* DataIn, size_t SizeIn) : Data(DataIn), Size(SizeIn) {}

    // 从字符串创建片段
    static BufferFragment FromString(const std::string& Str)
    {
        return BufferFragment(Str.data(), Str.size());
    }

    // 从字节数组创建片段
    static BufferFragment FromBytes(const std::vector<uint8_t>& Bytes)
    {
        return BufferFragment(Bytes.data(), Bytes.size());
    }

    // 从C字符串创建片段
    static BufferFragment FromCString(const char* Str)
    {
        return BufferFragment(Str, strlen(Str));
    }
};

// 零拷贝缓冲区（支持scatter-gather I/O）
class ZeroCopyBuffer
{
public:
    ZeroCopyBuffer() = default;
    ~ZeroCopyBuffer() = default;

    // 禁用拷贝，允许移动
    ZeroCopyBuffer(const ZeroCopyBuffer&) = delete;
    ZeroCopyBuffer& operator=(const ZeroCopyBuffer&) = delete;
    ZeroCopyBuffer(ZeroCopyBuffer&& Other) noexcept = default;
    ZeroCopyBuffer& operator=(ZeroCopyBuffer&& Other) noexcept = default;

    // 添加缓冲区片段
    void AddFragment(const BufferFragment& Fragment);
    void AddFragment(const void* Data, size_t Size);
    void AddFragment(const std::string& Str);
    void AddFragment(const std::vector<uint8_t>& Bytes);

    // 获取所有片段
    const std::vector<BufferFragment>& GetFragments() const
    {
        return Fragments;
    }

    // 获取总大小
    size_t GetTotalSize() const;

    // 清空缓冲区
    void Clear();

    // 检查是否为空
    bool IsEmpty() const
    {
        return Fragments.empty();
    }

    // 获取片段数量
    size_t GetFragmentCount() const
    {
        return Fragments.size();
    }

private:
    std::vector<BufferFragment> Fragments;
};

// 零拷贝读取缓冲区（支持从多个源读取）
class ZeroCopyReadBuffer
{
public:
    ZeroCopyReadBuffer() = default;
    ~ZeroCopyReadBuffer() = default;

    // 禁用拷贝，允许移动
    ZeroCopyReadBuffer(const ZeroCopyReadBuffer&) = delete;
    ZeroCopyReadBuffer& operator=(const ZeroCopyReadBuffer&) = delete;
    ZeroCopyReadBuffer(ZeroCopyReadBuffer&& Other) noexcept = default;
    ZeroCopyReadBuffer& operator=(ZeroCopyReadBuffer&& Other) noexcept = default;

    // 添加读取目标
    void AddTarget(void* Data, size_t Size);
    void AddTarget(std::vector<uint8_t>& Buffer);
    void AddTarget(std::string& Buffer);

    // 获取读取目标
    const std::vector<std::pair<void*, size_t>>& GetTargets() const
    {
        return Targets;
    }

    // 获取总目标大小
    size_t GetTotalTargetSize() const;

    // 清空目标
    void Clear();

    // 检查是否为空
    bool IsEmpty() const
    {
        return Targets.empty();
    }

    // 获取目标数量
    size_t GetTargetCount() const
    {
        return Targets.size();
    }

private:
    std::vector<std::pair<void*, size_t>> Targets;
};

// 零拷贝I/O操作结果
struct ZeroCopyResult
{
    size_t BytesTransferred = 0;
    bool Success = false;
    int ErrorCode = 0;

    ZeroCopyResult() = default;
    ZeroCopyResult(size_t Bytes, bool SuccessIn, int Error = 0)
        : BytesTransferred(Bytes), Success(SuccessIn), ErrorCode(Error)
    {
    }
};

// 零拷贝I/O工具类
class ZeroCopyIO
{
public:
    // 使用sendmsg进行零拷贝发送（支持scatter-gather）
    static ZeroCopyResult SendMsg(int Socket, const ZeroCopyBuffer& Buffer, int Flags = 0);

    // 使用recvmsg进行零拷贝接收（支持scatter-gather）
    static ZeroCopyResult RecvMsg(int Socket, ZeroCopyReadBuffer& Buffer, int Flags = 0);

    // 使用writev进行零拷贝写入
    static ZeroCopyResult WriteV(int Fd, const ZeroCopyBuffer& Buffer);

    // 使用readv进行零拷贝读取
    static ZeroCopyResult ReadV(int Fd, ZeroCopyReadBuffer& Buffer);

    // 检查系统是否支持零拷贝
    static bool IsSupported();

    // 获取零拷贝操作的性能统计
    struct PerformanceStats
    {
        size_t TotalOperations = 0;
        size_t TotalBytesTransferred = 0;
        double AverageBytesPerOperation = 0.0;
        size_t FailedOperations = 0;
    };

    static PerformanceStats GetStats();
    static void ResetStats();

private:
    static PerformanceStats Stats;
};

// 便利函数
inline ZeroCopyBuffer MakeZeroCopyBuffer()
{
    return ZeroCopyBuffer{};
}

inline ZeroCopyReadBuffer MakeZeroCopyReadBuffer()
{
    return ZeroCopyReadBuffer{};
}
}  // namespace Helianthus::Network::Asio
