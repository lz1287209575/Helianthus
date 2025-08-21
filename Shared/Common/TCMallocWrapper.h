#pragma once

#include <cstddef>
#include <memory>
#include <new>

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

    // TCMalloc 运行时配置
    struct RuntimeConfig
    {
        // 线程缓存配置
        size_t MaxTotalThreadCacheBytes = 64 * 1024 * 1024;    // 64MB 默认总线程缓存
        size_t MaxThreadCacheBytes = 4 * 1024 * 1024;          // 4MB 默认单线程缓存
        size_t ThreadCacheSize = 2 * 1024 * 1024;              // 2MB 线程缓存大小
        
        // 页堆配置
        size_t PageHeapFreeBytes = 256 * 1024 * 1024;          // 256MB 页堆空闲字节
        size_t PageHeapUnmapBytes = 128 * 1024 * 1024;         // 128MB 页堆取消映射阈值
        
        // 采样配置
        size_t SampleRate = 1024 * 1024;                       // 1MB 采样率
        bool EnableSampling = false;                           // 默认禁用采样
        
        // 性能配置
        bool EnableAggressiveDecommit = false;                 // 是否启用激进的内存释放
        bool EnableLargeAllocs = true;                         // 是否启用大内存分配优化
        size_t LargeAllocThreshold = 32 * 1024;                // 32KB 大内存分配阈值
        
        // 调试配置
        bool EnableDebugMode = false;                          // 调试模式
        bool EnableMemoryLeakCheck = false;                    // 内存泄漏检查
        size_t DebugAllocStackDepth = 0;                       // 调试分配栈深度
        
        // 统计配置
        bool EnableDetailedStats = false;                      // 详细统计
        bool EnablePerThreadStats = false;                     // 每线程统计
        
        // 垃圾回收配置
        size_t GCThreshold = 128 * 1024 * 1024;               // 128MB GC 触发阈值
        bool EnablePeriodicGC = false;                         // 定期 GC
        size_t GCIntervalMs = 30000;                           // 30秒 GC 间隔
    };

    // 运行时配置管理
    static bool SetRuntimeConfig(const RuntimeConfig& Config);
    static RuntimeConfig GetRuntimeConfig();
    static bool UpdateRuntimeConfig(const RuntimeConfig& Config);
    
    // 单独设置配置项
    static bool SetThreadCacheConfig(size_t MaxTotal, size_t MaxPerThread, size_t CacheSize);
    static bool SetPageHeapConfig(size_t FreeBytes, size_t UnmapBytes);
    static bool SetSamplingConfig(size_t SampleRate, bool EnableSampling);
    static bool SetPerformanceConfig(bool AggressiveDecommit, bool LargeAllocs, size_t LargeThreshold);
    static bool SetDebugConfig(bool DebugMode, bool LeakCheck, size_t StackDepth);
    
    // 运行时操作
    static void ForceGarbageCollection();
    static void ReleaseMemoryToSystem();
    static void FlushThreadCaches();
    
    // 高级统计
    struct AdvancedStats
    {
        size_t HeapSize = 0;
        size_t UnmappedBytes = 0;
        size_t PageHeapFreeBytes = 0;
        size_t PageHeapUnmappedBytes = 0;
        size_t TotalThreadCacheBytes = 0;
        size_t CentralCacheBytes = 0;
        size_t TransferCacheBytes = 0;
        size_t SpanCacheBytes = 0;
        size_t SampledObjects = 0;
        double FragmentationRatio = 0.0;
    };
    
    static AdvancedStats GetAdvancedStats();
};

// 全局 new/delete 操作符重载
// 注意：这些需要在全局命名空间中定义
}  // namespace Helianthus::Common

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

// 运行时配置便利宏
#define TCMALLOC_SET_CONFIG(config) Helianthus::Common::TCMallocWrapper::SetRuntimeConfig(config)
#define TCMALLOC_GET_CONFIG() Helianthus::Common::TCMallocWrapper::GetRuntimeConfig()
#define TCMALLOC_UPDATE_CONFIG(config) Helianthus::Common::TCMallocWrapper::UpdateRuntimeConfig(config)

// 运行时操作便利宏
#define TCMALLOC_FORCE_GC() Helianthus::Common::TCMallocWrapper::ForceGarbageCollection()
#define TCMALLOC_RELEASE_MEMORY() Helianthus::Common::TCMallocWrapper::ReleaseMemoryToSystem()
#define TCMALLOC_FLUSH_CACHES() Helianthus::Common::TCMallocWrapper::FlushThreadCaches()

// 高级统计便利宏
#define TCMALLOC_ADVANCED_STATS() Helianthus::Common::TCMallocWrapper::GetAdvancedStats()
