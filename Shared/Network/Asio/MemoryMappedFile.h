#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace Helianthus::Network::Asio
{
// 内存映射文件访问模式
enum class MappingMode
{
    ReadOnly,    // 只读模式
    ReadWrite,   // 读写模式
    WriteOnly    // 只写模式
};

// 内存映射文件类
class MemoryMappedFile
{
public:
    MemoryMappedFile() = default;
    ~MemoryMappedFile();

    // 禁用拷贝，允许移动
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& Other) noexcept;
    MemoryMappedFile& operator=(MemoryMappedFile&& Other) noexcept;

    // 映射文件到内存
    bool MapFile(const std::string& FilePath, MappingMode Mode, size_t Offset = 0, size_t Length = 0);

    // 取消映射
    void Unmap();

    // 获取映射的数据指针
    void* GetData() const { return MappedData; }
    const void* GetConstData() const { return MappedData; }

    // 获取映射的大小
    size_t GetSize() const { return MappedSize; }

    // 检查是否已映射
    bool IsMapped() const { return MappedData != nullptr; }

    // 同步内存到磁盘（仅对可写映射有效）
    bool Sync(bool Async = false);

    // 预取数据到内存
    bool Prefetch(size_t Offset = 0, size_t Length = 0);

    // 建议内存使用模式
    enum class AdviceMode
    {
        Normal,      // 正常访问模式
        Sequential,  // 顺序访问
        Random,      // 随机访问
        WillNeed,    // 即将需要
        DontNeed     // 不再需要
    };
    
    bool AdviseAccess(AdviceMode Mode, size_t Offset = 0, size_t Length = 0);

    // 获取文件大小（静态方法）
    static size_t GetFileSize(const std::string& FilePath);

    // 检查系统是否支持内存映射
    static bool IsSupported();

private:
    void* MappedData = nullptr;
    size_t MappedSize = 0;
    MappingMode Mode = MappingMode::ReadOnly;
    
#ifdef _WIN32
    HANDLE FileHandle = INVALID_HANDLE_VALUE;
    HANDLE MappingHandle = nullptr;
#else
    int FileDescriptor = -1;
#endif

    void CleanupResources();
};

// 内存映射缓冲区片段（用于零拷贝操作）
class MemoryMappedBufferFragment
{
public:
    MemoryMappedBufferFragment() = default;
    MemoryMappedBufferFragment(std::shared_ptr<MemoryMappedFile> File, size_t Offset, size_t Size);

    // 获取数据指针
    const void* GetData() const;
    void* GetMutableData() const;

    // 获取大小
    size_t GetSize() const { return Size; }

    // 获取偏移量
    size_t GetOffset() const { return Offset; }

    // 检查是否有效
    bool IsValid() const { return File && File->IsMapped() && Size > 0; }

    // 获取底层文件
    std::shared_ptr<MemoryMappedFile> GetFile() const { return File; }

private:
    std::shared_ptr<MemoryMappedFile> File;
    size_t Offset = 0;
    size_t Size = 0;
};

// 大文件传输优化器
class LargeFileTransferOptimizer
{
public:
    struct TransferConfig
    {
        size_t ChunkSize = 64 * 1024;        // 64KB 块大小
        size_t MaxConcurrentChunks = 4;      // 最大并发块数
        bool UseMemoryMapping = true;        // 使用内存映射
        bool UsePrefetch = true;             // 使用预取
        bool UseSequentialAccess = true;     // 使用顺序访问提示
    };

    static TransferConfig GetOptimalConfig(size_t FileSize);
    
    // 为大文件传输创建优化的缓冲区片段
    static std::vector<MemoryMappedBufferFragment> CreateOptimizedFragments(
        const std::string& FilePath, 
        const TransferConfig& Config);

    // 检查文件是否适合内存映射
    static bool ShouldUseMemoryMapping(size_t FileSize);

    // 获取系统内存信息
    struct MemoryInfo
    {
        size_t TotalPhysicalMemory = 0;
        size_t AvailablePhysicalMemory = 0;
        size_t TotalVirtualMemory = 0;
        size_t AvailableVirtualMemory = 0;
    };
    
    static MemoryInfo GetSystemMemoryInfo();
};

}  // namespace Helianthus::Network::Asio
