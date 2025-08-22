#include "Shared/Network/Asio/MemoryMappedFile.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include <cstring>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
#endif

namespace Helianthus::Network::Asio
{

// MemoryMappedFile 实现
MemoryMappedFile::~MemoryMappedFile()
{
    Unmap();
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& Other) noexcept
    : MappedData(Other.MappedData), MappedSize(Other.MappedSize), Mode(Other.Mode)
{
#ifdef _WIN32
    FileHandle = Other.FileHandle;
    MappingHandle = Other.MappingHandle;
    Other.FileHandle = INVALID_HANDLE_VALUE;
    Other.MappingHandle = nullptr;
#else
    FileDescriptor = Other.FileDescriptor;
    Other.FileDescriptor = -1;
#endif
    
    Other.MappedData = nullptr;
    Other.MappedSize = 0;
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& Other) noexcept
{
    if (this != &Other)
    {
        // 清理当前资源
        Unmap();
        
        // 移动资源
        MappedData = Other.MappedData;
        MappedSize = Other.MappedSize;
        Mode = Other.Mode;
        
#ifdef _WIN32
        FileHandle = Other.FileHandle;
        MappingHandle = Other.MappingHandle;
        Other.FileHandle = INVALID_HANDLE_VALUE;
        Other.MappingHandle = nullptr;
#else
        FileDescriptor = Other.FileDescriptor;
        Other.FileDescriptor = -1;
#endif
        
        Other.MappedData = nullptr;
        Other.MappedSize = 0;
    }
    return *this;
}

bool MemoryMappedFile::MapFile(const std::string& FilePath, MappingMode MappingMode, size_t Offset, size_t Length)
{
    // 先取消之前的映射
    Unmap();
    
    Mode = MappingMode;

#ifdef _WIN32
    // Windows 实现
    DWORD FileAccess = 0;
    DWORD MappingProtection = 0;
    DWORD ViewAccess = 0;
    
    switch (MappingMode)
    {
        case MappingMode::ReadOnly:
            FileAccess = GENERIC_READ;
            MappingProtection = PAGE_READONLY;
            ViewAccess = FILE_MAP_READ;
            break;
        case MappingMode::ReadWrite:
            FileAccess = GENERIC_READ | GENERIC_WRITE;
            MappingProtection = PAGE_READWRITE;
            ViewAccess = FILE_MAP_ALL_ACCESS;
            break;
        case MappingMode::WriteOnly:
            FileAccess = GENERIC_WRITE;
            MappingProtection = PAGE_READWRITE;
            ViewAccess = FILE_MAP_WRITE;
            break;
    }
    
    // 打开文件
    FileHandle = CreateFileA(FilePath.c_str(), FileAccess, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (FileHandle == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open file: " << FilePath << ", Error: " << GetLastError() << std::endl;
        return false;
    }
    
    // 获取文件大小
    LARGE_INTEGER FileSize;
    if (!GetFileSizeEx(FileHandle, &FileSize))
    {
        std::cerr << "Failed to get file size, Error: " << GetLastError() << std::endl;
        CleanupResources();
        return false;
    }
    
    size_t ActualFileSize = static_cast<size_t>(FileSize.QuadPart);
    
    // 计算映射大小
    if (Length == 0)
    {
        Length = ActualFileSize - Offset;
    }
    
    if (Offset + Length > ActualFileSize)
    {
        std::cerr << "Mapping range exceeds file size" << std::endl;
        CleanupResources();
        return false;
    }
    
    // 对于完整文件映射，不需要对齐处理
    DWORD AlignedOffset = static_cast<DWORD>(Offset);
    DWORD OffsetDelta = 0;
    DWORD AlignedLength = static_cast<DWORD>(Length);
    
    // 对于部分映射或大文件，需要对齐到分配粒度
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    DWORD AllocationGranularity = SysInfo.dwAllocationGranularity;
    
    if ((Offset > 0 || Length < ActualFileSize) && ActualFileSize >= AllocationGranularity)
    {
        // 计算对齐后的偏移量和大小
        AlignedOffset = static_cast<DWORD>(Offset / AllocationGranularity) * AllocationGranularity;
        OffsetDelta = static_cast<DWORD>(Offset - AlignedOffset);
        AlignedLength = static_cast<DWORD>((Length + OffsetDelta + AllocationGranularity - 1) / AllocationGranularity) * AllocationGranularity;
        
        // Debug output removed for production
    }
    
    MappedSize = Length;  // 保持原始大小用于用户访问
    
    // 创建文件映射对象
    MappingHandle = CreateFileMappingA(FileHandle, nullptr, MappingProtection, 0, 0, nullptr);
    if (MappingHandle == nullptr)
    {
        std::cerr << "Failed to create file mapping, Error: " << GetLastError() << std::endl;
        CleanupResources();
        return false;
    }
    
    // 映射视图
    MappedData = MapViewOfFile(MappingHandle, ViewAccess, 
                              static_cast<DWORD>((static_cast<ULONGLONG>(AlignedOffset) >> 32) & 0xFFFFFFFF), 
                              static_cast<DWORD>(AlignedOffset & 0xFFFFFFFF), 
                              AlignedLength);
    
    if (MappedData == nullptr)
    {
        std::cerr << "Failed to map view of file, Error: " << GetLastError() << std::endl;
        CleanupResources();
        return false;
    }
    
#else
    // POSIX 实现
    int FileFlags = 0;
    int MmapProtection = 0;
    
    switch (MappingMode)
    {
        case MappingMode::ReadOnly:
            FileFlags = O_RDONLY;
            MmapProtection = PROT_READ;
            break;
        case MappingMode::ReadWrite:
            FileFlags = O_RDWR;
            MmapProtection = PROT_READ | PROT_WRITE;
            break;
        case MappingMode::WriteOnly:
            FileFlags = O_WRONLY;
            MmapProtection = PROT_WRITE;
            break;
    }
    
    // 打开文件
    FileDescriptor = open(FilePath.c_str(), FileFlags);
    if (FileDescriptor == -1)
    {
        std::cerr << "Failed to open file: " << FilePath << ", Error: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 获取文件大小
    struct stat FileStats;
    if (fstat(FileDescriptor, &FileStats) == -1)
    {
        std::cerr << "Failed to get file stats, Error: " << strerror(errno) << std::endl;
        CleanupResources();
        return false;
    }
    
    size_t ActualFileSize = static_cast<size_t>(FileStats.st_size);
    
    // 计算映射大小
    if (Length == 0)
    {
        Length = ActualFileSize - Offset;
    }
    
    if (Offset + Length > ActualFileSize)
    {
        std::cerr << "Mapping range exceeds file size" << std::endl;
        CleanupResources();
        return false;
    }
    
    MappedSize = Length;
    
    // 执行内存映射
    MappedData = mmap(nullptr, Length, MmapProtection, MAP_SHARED, FileDescriptor, static_cast<off_t>(Offset));
    
    if (MappedData == MAP_FAILED)
    {
        std::cerr << "Failed to map file, Error: " << strerror(errno) << std::endl;
        MappedData = nullptr;
        CleanupResources();
        return false;
    }
#endif

    return true;
}

void MemoryMappedFile::Unmap()
{
    if (MappedData)
    {
#ifdef _WIN32
        UnmapViewOfFile(MappedData);
#else
        munmap(MappedData, MappedSize);
#endif
        MappedData = nullptr;
        MappedSize = 0;
    }
    
    CleanupResources();
}

bool MemoryMappedFile::Sync(bool Async)
{
    if (!IsMapped())
    {
        return false;
    }
    
#ifdef _WIN32
    return FlushViewOfFile(MappedData, MappedSize) != 0;
#else
    int Flags = Async ? MS_ASYNC : MS_SYNC;
    return msync(MappedData, MappedSize, Flags) == 0;
#endif
}

bool MemoryMappedFile::Prefetch(size_t Offset, size_t Length)
{
    if (!IsMapped())
    {
        return false;
    }
    
    if (Length == 0)
    {
        Length = MappedSize - Offset;
    }
    
    if (Offset + Length > MappedSize)
    {
        return false;
    }
    
#ifdef _WIN32
    // Windows: 简单地触摸内存页面来实现预取效果
    volatile char* Ptr = static_cast<char*>(MappedData) + Offset;
    size_t PageSize = 4096;  // 假设 4KB 页面大小
    
    for (size_t i = 0; i < Length; i += PageSize)
    {
        volatile char Touch = Ptr[i];
        (void)Touch;  // 避免未使用变量警告
    }
    
    return true;
#else
    // POSIX: 使用 madvise
    return madvise(static_cast<char*>(MappedData) + Offset, Length, MADV_WILLNEED) == 0;
#endif
}

bool MemoryMappedFile::AdviseAccess(AdviceMode AdviceMode, size_t Offset, size_t Length)
{
    if (!IsMapped())
    {
        return false;
    }
    
    if (Length == 0)
    {
        Length = MappedSize - Offset;
    }
    
    if (Offset + Length > MappedSize)
    {
        return false;
    }
    
#ifdef _WIN32
    // Windows 没有直接的 madvise 等价物，这里返回 true
    return true;
#else
    int Advice = 0;
    switch (AdviceMode)
    {
        case AdviceMode::Normal:
            Advice = MADV_NORMAL;
            break;
        case AdviceMode::Sequential:
            Advice = MADV_SEQUENTIAL;
            break;
        case AdviceMode::Random:
            Advice = MADV_RANDOM;
            break;
        case AdviceMode::WillNeed:
            Advice = MADV_WILLNEED;
            break;
        case AdviceMode::DontNeed:
            Advice = MADV_DONTNEED;
            break;
    }
    
    return madvise(static_cast<char*>(MappedData) + Offset, Length, Advice) == 0;
#endif
}

size_t MemoryMappedFile::GetFileSize(const std::string& FilePath)
{
#ifdef _WIN32
    HANDLE FileHandle = CreateFileA(FilePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (FileHandle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    
    LARGE_INTEGER FileSize;
    if (!GetFileSizeEx(FileHandle, &FileSize))
    {
        CloseHandle(FileHandle);
        return 0;
    }
    
    CloseHandle(FileHandle);
    return static_cast<size_t>(FileSize.QuadPart);
#else
    struct stat FileStats;
    if (stat(FilePath.c_str(), &FileStats) == -1)
    {
        return 0;
    }
    
    return static_cast<size_t>(FileStats.st_size);
#endif
}

bool MemoryMappedFile::IsSupported()
{
    // 内存映射在现代操作系统上都支持
    return true;
}

void MemoryMappedFile::CleanupResources()
{
#ifdef _WIN32
    if (MappingHandle)
    {
        CloseHandle(MappingHandle);
        MappingHandle = nullptr;
    }
    
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(FileHandle);
        FileHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (FileDescriptor != -1)
    {
        close(FileDescriptor);
        FileDescriptor = -1;
    }
#endif
}

// MemoryMappedBufferFragment 实现
MemoryMappedBufferFragment::MemoryMappedBufferFragment(std::shared_ptr<MemoryMappedFile> FilePtr, size_t OffsetIn, size_t SizeIn)
    : File(std::move(FilePtr)), Offset(OffsetIn), Size(SizeIn)
{
    // 验证参数
    if (File && File->IsMapped())
    {
        if (Offset + Size > File->GetSize())
        {
            Size = File->GetSize() - Offset;
        }
    }
}

const void* MemoryMappedBufferFragment::GetData() const
{
    if (!IsValid())
    {
        return nullptr;
    }
    
    return static_cast<const char*>(File->GetConstData()) + Offset;
}

void* MemoryMappedBufferFragment::GetMutableData() const
{
    if (!IsValid())
    {
        return nullptr;
    }
    
    return static_cast<char*>(File->GetData()) + Offset;
}

// LargeFileTransferOptimizer 实现
LargeFileTransferOptimizer::TransferConfig LargeFileTransferOptimizer::GetOptimalConfig(size_t FileSize)
{
    TransferConfig Config;
    
    // 根据文件大小调整配置
    if (FileSize < 1024 * 1024)  // < 1MB
    {
        Config.ChunkSize = 16 * 1024;  // 16KB
        Config.MaxConcurrentChunks = 2;
        Config.UseMemoryMapping = false;  // 小文件不使用内存映射
    }
    else if (FileSize < 100 * 1024 * 1024)  // < 100MB
    {
        Config.ChunkSize = 64 * 1024;  // 64KB
        Config.MaxConcurrentChunks = 4;
        Config.UseMemoryMapping = true;
    }
    else  // >= 100MB
    {
        Config.ChunkSize = 256 * 1024;  // 256KB
        Config.MaxConcurrentChunks = 8;
        Config.UseMemoryMapping = true;
    }
    
    return Config;
}

std::vector<MemoryMappedBufferFragment> LargeFileTransferOptimizer::CreateOptimizedFragments(
    const std::string& FilePath, const TransferConfig& Config)
{
    std::vector<MemoryMappedBufferFragment> Fragments;
    
    size_t FileSize = MemoryMappedFile::GetFileSize(FilePath);
    if (FileSize == 0)
    {
        return Fragments;
    }
    
    if (!Config.UseMemoryMapping || !ShouldUseMemoryMapping(FileSize))
    {
        // 不使用内存映射，返回空列表
        return Fragments;
    }
    
    // 创建内存映射文件
    auto MappedFile = std::make_shared<MemoryMappedFile>();
    if (!MappedFile->MapFile(FilePath, MappingMode::ReadOnly))
    {
        return Fragments;
    }
    
    // 设置访问模式建议
    if (Config.UseSequentialAccess)
    {
        MappedFile->AdviseAccess(MemoryMappedFile::AdviceMode::Sequential);
    }
    
    // 预取第一块数据
    if (Config.UsePrefetch)
    {
        size_t PrefetchSize = std::min(Config.ChunkSize * Config.MaxConcurrentChunks, FileSize);
        MappedFile->Prefetch(0, PrefetchSize);
    }
    
    // 创建片段
    for (size_t Offset = 0; Offset < FileSize; Offset += Config.ChunkSize)
    {
        size_t ChunkSize = std::min(Config.ChunkSize, FileSize - Offset);
        Fragments.emplace_back(MappedFile, Offset, ChunkSize);
    }
    
    return Fragments;
}

bool LargeFileTransferOptimizer::ShouldUseMemoryMapping(size_t FileSize)
{
    // 获取系统内存信息
    MemoryInfo MemInfo = GetSystemMemoryInfo();
    
    // 如果文件大小超过可用内存的 50%，不使用内存映射
    if (FileSize > MemInfo.AvailablePhysicalMemory / 2)
    {
        return false;
    }
    
    // 如果文件小于 64KB，不使用内存映射
    if (FileSize < 64 * 1024)
    {
        return false;
    }
    
    return true;
}

LargeFileTransferOptimizer::MemoryInfo LargeFileTransferOptimizer::GetSystemMemoryInfo()
{
    MemoryInfo Info;
    
#ifdef _WIN32
    MEMORYSTATUSEX MemStatus;
    MemStatus.dwLength = sizeof(MemStatus);
    if (GlobalMemoryStatusEx(&MemStatus))
    {
        Info.TotalPhysicalMemory = static_cast<size_t>(MemStatus.ullTotalPhys);
        Info.AvailablePhysicalMemory = static_cast<size_t>(MemStatus.ullAvailPhys);
        Info.TotalVirtualMemory = static_cast<size_t>(MemStatus.ullTotalVirtual);
        Info.AvailableVirtualMemory = static_cast<size_t>(MemStatus.ullAvailVirtual);
    }
#else
    // POSIX: 使用 sysconf 获取内存信息
    long PageSize = sysconf(_SC_PAGESIZE);
    long TotalPages = sysconf(_SC_PHYS_PAGES);
    long AvailablePages = sysconf(_SC_AVPHYS_PAGES);
    
    if (PageSize > 0 && TotalPages > 0)
    {
        Info.TotalPhysicalMemory = static_cast<size_t>(PageSize * TotalPages);
    }
    
    if (PageSize > 0 && AvailablePages > 0)
    {
        Info.AvailablePhysicalMemory = static_cast<size_t>(PageSize * AvailablePages);
    }
    
    // 虚拟内存信息在 POSIX 中较难获取，这里设置为物理内存的两倍作为估计
    Info.TotalVirtualMemory = Info.TotalPhysicalMemory * 2;
    Info.AvailableVirtualMemory = Info.AvailablePhysicalMemory * 2;
#endif
    
    return Info;
}

}  // namespace Helianthus::Network::Asio
