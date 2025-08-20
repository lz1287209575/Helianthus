#pragma once

#include <cstddef>
#include <new>
#include <memory>

// TCMalloc 配置选项
// 定义 HELIANTHUS_USE_TCMALLOC 来启用真正的 TCMalloc
#ifndef HELIANTHUS_USE_TCMALLOC
    // 默认不使用 TCMalloc，使用标准库实现
    // 当 gperftools 构建成功后，可以在 BUILD.bazel 中定义这个宏
#endif

namespace Helianthus::Common
{
    // TCMalloc 包装器类
    class TCMallocWrapper
    {
    public:
        // 初始化 TCMalloc
        static bool Initialize();
        
        // 清理 TCMalloc
        static void Shutdown();
        
        // 检查是否已初始化
        static bool IsInitialized();
        
        // 内存分配函数
        static void* Malloc(size_t Size);
        static void* Calloc(size_t Count, size_t Size);
        static void* Realloc(void* Ptr, size_t NewSize);
        static void Free(void* Ptr);
        
        // C++ 风格的分配函数
        static void* New(size_t Size);
        static void Delete(void* Ptr);
        static void DeleteArray(void* Ptr);
        
        // 对齐分配
        static void* AlignedMalloc(size_t Size, size_t Alignment);
        static void AlignedFree(void* Ptr);
        
        // 内存统计
        struct MemoryStats
        {
            size_t TotalAllocated = 0;
            size_t TotalFreed = 0;
            size_t CurrentUsage = 0;
            size_t PeakUsage = 0;
            size_t AllocatedBlocks = 0;
            size_t FreedBlocks = 0;
        };
        
        static MemoryStats GetStats();
        static void ResetStats();
        
        // 内存泄漏检测
        static bool IsMemoryLeakDetected();
        static void DumpMemoryLeaks();
        
        // 性能调优
        static void SetMaxTotalThreadCacheBytes(size_t Bytes);
        static void SetMaxThreadCacheBytes(size_t Bytes);
        static void SetMaxCacheSize(size_t Bytes);
        
        // 获取当前线程的缓存统计
        struct ThreadCacheStats
        {
            size_t CacheSize = 0;
            size_t CacheHitRate = 0;
            size_t AllocatedBytes = 0;
            size_t FreedBytes = 0;
        };
        
        static ThreadCacheStats GetThreadCacheStats();
    };

    // 全局 new/delete 操作符重载
    // 注意：这些需要在全局命名空间中定义
}

// 全局 new/delete 操作符重载
void* operator new(std::size_t Size);
void* operator new[](std::size_t Size);
void* operator new(std::size_t Size, const std::nothrow_t&) noexcept;
void* operator new[](std::size_t Size, const std::nothrow_t&) noexcept;

void operator delete(void* Ptr) noexcept;
void operator delete[](void* Ptr) noexcept;
void operator delete(void* Ptr, const std::nothrow_t&) noexcept;
void operator delete[](void* Ptr, const std::nothrow_t&) noexcept;

// C++17 对齐分配支持
void* operator new(std::size_t Size, std::align_val_t Alignment);
void* operator new[](std::size_t Size, std::align_val_t Alignment);
void* operator new(std::size_t Size, std::align_val_t Alignment, const std::nothrow_t&) noexcept;
void* operator new[](std::size_t Size, std::align_val_t Alignment, const std::nothrow_t&) noexcept;

void operator delete(void* Ptr, std::align_val_t Alignment) noexcept;
void operator delete[](void* Ptr, std::align_val_t Alignment) noexcept;
void operator delete(void* Ptr, std::align_val_t Alignment, const std::nothrow_t&) noexcept;
void operator delete[](void* Ptr, std::align_val_t Alignment, const std::nothrow_t&) noexcept;

// 便利宏
#define TCMALLOC_INIT() Helianthus::Common::TCMallocWrapper::Initialize()
#define TCMALLOC_SHUTDOWN() Helianthus::Common::TCMallocWrapper::Shutdown()
#define TCMALLOC_STATS() Helianthus::Common::TCMallocWrapper::GetStats()
#define TCMALLOC_RESET_STATS() Helianthus::Common::TCMallocWrapper::ResetStats()
