#include "Shared/Common/TCMallocWrapper.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>

// 条件编译：如果有 TCMalloc 头文件，就使用它
#ifdef HELIANTHUS_USE_TCMALLOC
    // 当真正的 TCMalloc 可用时，取消这些注释
    #include <tcmalloc/malloc_extension.h>
    #include <tcmalloc/tcmalloc.h>
    // #include <gperftools/profiler.h>
    #define USE_REAL_TCMALLOC 1  // 启用真正的 TCMalloc
#else
    #define USE_REAL_TCMALLOC 0
#endif

namespace Helianthus::Common
{
// 静态成员变量
static std::atomic<bool> GInitialized = false;
static std::mutex GInitMutex;
static std::atomic<size_t> GTotalAllocated = 0;
static std::atomic<size_t> GTotalFreed = 0;
static std::atomic<size_t> GAllocatedBlocks = 0;
static std::atomic<size_t> GFreedBlocks = 0;
static std::atomic<size_t> GPeakUsage = 0;

// 运行时配置
static std::mutex GConfigMutex;
static TCMallocWrapper::RuntimeConfig GCurrentConfig;

// TCMallocWrapper 实现
bool TCMallocWrapper::Initialize()
{
    std::lock_guard<std::mutex> lock(GInitMutex);

    if (GInitialized.load())
    {
        return true;
    }

    // 初始化 TCMalloc 并应用默认配置
#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        // 应用默认配置
        Extension->SetNumericProperty("tcmalloc.max_total_thread_cache_bytes",
                                      GCurrentConfig.MaxTotalThreadCacheBytes);
        Extension->SetNumericProperty("tcmalloc.max_per_cpu_cache_size",
                                      GCurrentConfig.MaxThreadCacheBytes);
        Extension->SetNumericProperty("tcmalloc.page_heap_free_bytes",
                                      GCurrentConfig.PageHeapFreeBytes);
        Extension->SetNumericProperty("tcmalloc.page_heap_unmapped_bytes",
                                      GCurrentConfig.PageHeapUnmapBytes);

        // 默认禁用采样
        if (!GCurrentConfig.EnableSampling)
        {
            Extension->SetNumericProperty("tcmalloc.sampling_rate", 0);
        }
    }
#endif

    GInitialized.store(true);
    return true;
}

void TCMallocWrapper::Shutdown()
{
    std::lock_guard<std::mutex> lock(GInitMutex);

    if (!GInitialized.load())
    {
        return;
    }

    // 输出最终统计信息
    auto Stats = GetStats();
    if (Stats.CurrentUsage > 0)
    {
        // 这里可以记录内存泄漏警告
    }

    GInitialized.store(false);
}

bool TCMallocWrapper::IsInitialized()
{
    return GInitialized.load();
}

void* TCMallocWrapper::Malloc(size_t Size)
{
    if (!GInitialized.load())
    {
        Initialize();
    }

    void* Ptr = nullptr;

#if USE_REAL_TCMALLOC
    // 使用真正的 TCMalloc
    Ptr = tcmalloc::tc_malloc(Size);
#else
    // 使用标准库
    Ptr = malloc(Size);
#endif

    if (Ptr)
    {
        GTotalAllocated.fetch_add(Size);
        GAllocatedBlocks.fetch_add(1);

        size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
        size_t PeakUsage = GPeakUsage.load();
        while (CurrentUsage > PeakUsage &&
               !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage))
        {
            // 重试直到成功更新峰值
        }
    }

    return Ptr;
}

void* TCMallocWrapper::Calloc(size_t Count, size_t Size)
{
    if (!GInitialized.load())
    {
        Initialize();
    }

    void* Ptr = calloc(Count, Size);
    if (Ptr)
    {
        size_t TotalSize = Count * Size;
        GTotalAllocated.fetch_add(TotalSize);
        GAllocatedBlocks.fetch_add(1);

        size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
        size_t PeakUsage = GPeakUsage.load();
        while (CurrentUsage > PeakUsage &&
               !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage))
        {
            // 重试直到成功更新峰值
        }
    }

    return Ptr;
}

void* TCMallocWrapper::Realloc(void* Ptr, size_t NewSize)
{
    if (!GInitialized.load())
    {
        Initialize();
    }

    // 注意：这里我们无法准确获取旧大小，所以统计可能不准确
    void* NewPtr = realloc(Ptr, NewSize);
    if (NewPtr)
    {
        // 简化处理：假设重新分配成功
        GTotalAllocated.fetch_add(NewSize);
        GAllocatedBlocks.fetch_add(1);

        size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
        size_t PeakUsage = GPeakUsage.load();
        while (CurrentUsage > PeakUsage &&
               !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage))
        {
            // 重试直到成功更新峰值
        }
    }

    return NewPtr;
}

void TCMallocWrapper::Free(void* Ptr)
{
    if (!Ptr)
    {
        return;
    }

    size_t Size = 0;

#if USE_REAL_TCMALLOC
    // 使用真正的 TCMalloc
    Size = tcmalloc::tc_malloc_size(Ptr);
    tcmalloc::tc_free(Ptr);
#else
    // 使用标准库 - 无法获取准确大小，使用估算值
    Size = 1024;  // 假设平均 1KB
    free(Ptr);
#endif

    GTotalFreed.fetch_add(Size);
    GFreedBlocks.fetch_add(1);
}

void* TCMallocWrapper::New(size_t Size)
{
    return Malloc(Size);
}

void TCMallocWrapper::Delete(void* Ptr)
{
    Free(Ptr);
}

void TCMallocWrapper::DeleteArray(void* Ptr)
{
    Free(Ptr);
}

void* TCMallocWrapper::AlignedMalloc(size_t Size, size_t Alignment)
{
    if (!GInitialized.load())
    {
        Initialize();
    }

    void* Ptr = nullptr;
#if defined(_MSC_VER)
    Ptr = _aligned_malloc(Size, Alignment);
#else
    Ptr = aligned_alloc(Alignment, Size);
#endif
    if (Ptr)
    {
        GTotalAllocated.fetch_add(Size);
        GAllocatedBlocks.fetch_add(1);

        size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
        size_t PeakUsage = GPeakUsage.load();
        while (CurrentUsage > PeakUsage &&
               !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage))
        {
            // 重试直到成功更新峰值
        }
    }

    return Ptr;
}

void TCMallocWrapper::AlignedFree(void* Ptr)
{
    if (!Ptr)
        return;
#if defined(_MSC_VER)
    _aligned_free(Ptr);
#else
    free(Ptr);
#endif
}

TCMallocWrapper::MemoryStats TCMallocWrapper::GetStats()
{
    MemoryStats Stats;

    if (GInitialized.load())
    {
        Stats.TotalAllocated = GTotalAllocated.load();
        Stats.TotalFreed = GTotalFreed.load();
        Stats.CurrentUsage = Stats.TotalAllocated - Stats.TotalFreed;
        Stats.PeakUsage = GPeakUsage.load();
        Stats.AllocatedBlocks = GAllocatedBlocks.load();
        Stats.FreedBlocks = GFreedBlocks.load();
    }

    return Stats;
}

void TCMallocWrapper::ResetStats()
{
    GTotalAllocated.store(0);
    GTotalFreed.store(0);
    GAllocatedBlocks.store(0);
    GFreedBlocks.store(0);
    GPeakUsage.store(0);
}

bool TCMallocWrapper::IsMemoryLeakDetected()
{
    auto Stats = GetStats();
    return Stats.CurrentUsage > 0;
}

void TCMallocWrapper::DumpMemoryLeaks()
{
    // 暂时不实现
}

void TCMallocWrapper::SetMaxTotalThreadCacheBytes(size_t Bytes)
{
    // 暂时不实现
}

void TCMallocWrapper::SetMaxThreadCacheBytes(size_t Bytes)
{
    // 暂时不实现
}

void TCMallocWrapper::SetMaxCacheSize(size_t Bytes)
{
    // 暂时不实现
}

TCMallocWrapper::ThreadCacheStats TCMallocWrapper::GetThreadCacheStats()
{
    ThreadCacheStats Stats;
#if USE_REAL_TCMALLOC
    // 使用真正的 TCMalloc API 获取线程缓存统计
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        size_t Value = 0;
        if (Extension->GetNumericProperty("tcmalloc.current_total_thread_cache_bytes", &Value))
        {
            Stats.CacheSize = Value;
        }
        if (Extension->GetNumericProperty("tcmalloc.thread_cache_free_bytes", &Value))
        {
            Stats.AllocatedBytes = Value;
        }
    }
#endif
    return Stats;
}

// 运行时配置管理实现
bool TCMallocWrapper::SetRuntimeConfig(const RuntimeConfig& Config)
{
    std::lock_guard<std::mutex> lock(GConfigMutex);

    GCurrentConfig = Config;

#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (!Extension)
    {
        return false;
    }

    // 应用线程缓存配置
    Extension->SetNumericProperty("tcmalloc.max_total_thread_cache_bytes",
                                  Config.MaxTotalThreadCacheBytes);
    Extension->SetNumericProperty("tcmalloc.max_per_cpu_cache_size", Config.MaxThreadCacheBytes);

    // 应用页堆配置
    Extension->SetNumericProperty("tcmalloc.page_heap_free_bytes", Config.PageHeapFreeBytes);
    Extension->SetNumericProperty("tcmalloc.page_heap_unmapped_bytes", Config.PageHeapUnmapBytes);

    // 应用采样配置
    if (Config.EnableSampling)
    {
        Extension->SetNumericProperty("tcmalloc.sampling_rate", Config.SampleRate);
    }
    else
    {
        Extension->SetNumericProperty("tcmalloc.sampling_rate", 0);
    }

    return true;
#else
    // 标准库实现，仅保存配置
    return true;
#endif
}

TCMallocWrapper::RuntimeConfig TCMallocWrapper::GetRuntimeConfig()
{
    std::lock_guard<std::mutex> lock(GConfigMutex);
    return GCurrentConfig;
}

bool TCMallocWrapper::UpdateRuntimeConfig(const RuntimeConfig& Config)
{
    return SetRuntimeConfig(Config);
}

bool TCMallocWrapper::SetThreadCacheConfig(size_t MaxTotal, size_t MaxPerThread, size_t CacheSize)
{
    std::lock_guard<std::mutex> lock(GConfigMutex);

    GCurrentConfig.MaxTotalThreadCacheBytes = MaxTotal;
    GCurrentConfig.MaxThreadCacheBytes = MaxPerThread;
    GCurrentConfig.ThreadCacheSize = CacheSize;

#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        Extension->SetNumericProperty("tcmalloc.max_total_thread_cache_bytes", MaxTotal);
        Extension->SetNumericProperty("tcmalloc.max_per_cpu_cache_size", MaxPerThread);
        return true;
    }
    return false;
#else
    return true;
#endif
}

bool TCMallocWrapper::SetPageHeapConfig(size_t FreeBytes, size_t UnmapBytes)
{
    std::lock_guard<std::mutex> lock(GConfigMutex);

    GCurrentConfig.PageHeapFreeBytes = FreeBytes;
    GCurrentConfig.PageHeapUnmapBytes = UnmapBytes;

#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        Extension->SetNumericProperty("tcmalloc.page_heap_free_bytes", FreeBytes);
        Extension->SetNumericProperty("tcmalloc.page_heap_unmapped_bytes", UnmapBytes);
        return true;
    }
    return false;
#else
    return true;
#endif
}

bool TCMallocWrapper::SetSamplingConfig(size_t SampleRate, bool EnableSampling)
{
    std::lock_guard<std::mutex> lock(GConfigMutex);

    GCurrentConfig.SampleRate = SampleRate;
    GCurrentConfig.EnableSampling = EnableSampling;

#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        Extension->SetNumericProperty("tcmalloc.sampling_rate", EnableSampling ? SampleRate : 0);
        return true;
    }
    return false;
#else
    return true;
#endif
}

bool TCMallocWrapper::SetPerformanceConfig(bool AggressiveDecommit,
                                           bool LargeAllocs,
                                           size_t LargeThreshold)
{
    std::lock_guard<std::mutex> lock(GConfigMutex);

    GCurrentConfig.EnableAggressiveDecommit = AggressiveDecommit;
    GCurrentConfig.EnableLargeAllocs = LargeAllocs;
    GCurrentConfig.LargeAllocThreshold = LargeThreshold;

    // 这些配置主要影响内部算法，在标准库模式下也可以保存
    return true;
}

bool TCMallocWrapper::SetDebugConfig(bool DebugMode, bool LeakCheck, size_t StackDepth)
{
    std::lock_guard<std::mutex> lock(GConfigMutex);

    GCurrentConfig.EnableDebugMode = DebugMode;
    GCurrentConfig.EnableMemoryLeakCheck = LeakCheck;
    GCurrentConfig.DebugAllocStackDepth = StackDepth;

    // 调试配置在标准库模式下也可以保存
    return true;
}

void TCMallocWrapper::ForceGarbageCollection()
{
#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        Extension->ReleaseMemoryToSystem(0);  // 释放所有可能的内存
    }
#endif
    // 标准库模式下无操作
}

void TCMallocWrapper::ReleaseMemoryToSystem()
{
#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        Extension->ReleaseMemoryToSystem(0);
    }
#endif
    // 标准库模式下无操作
}

void TCMallocWrapper::FlushThreadCaches()
{
#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        Extension->MarkThreadIdle();
    }
#endif
    // 标准库模式下无操作
}

TCMallocWrapper::AdvancedStats TCMallocWrapper::GetAdvancedStats()
{
    AdvancedStats Stats;

#if USE_REAL_TCMALLOC
    auto Extension = tcmalloc::MallocExtension::instance();
    if (Extension)
    {
        size_t Value = 0;

        if (Extension->GetNumericProperty("generic.heap_size", &Value))
        {
            Stats.HeapSize = Value;
        }
        if (Extension->GetNumericProperty("tcmalloc.page_heap_free_bytes", &Value))
        {
            Stats.PageHeapFreeBytes = Value;
        }
        if (Extension->GetNumericProperty("tcmalloc.page_heap_unmapped_bytes", &Value))
        {
            Stats.PageHeapUnmappedBytes = Value;
        }
        if (Extension->GetNumericProperty("tcmalloc.current_total_thread_cache_bytes", &Value))
        {
            Stats.TotalThreadCacheBytes = Value;
        }
        if (Extension->GetNumericProperty("tcmalloc.central_cache_free_bytes", &Value))
        {
            Stats.CentralCacheBytes = Value;
        }

        // 计算碎片率
        if (Stats.HeapSize > 0)
        {
            size_t UsedBytes = Stats.HeapSize - Stats.PageHeapFreeBytes - Stats.UnmappedBytes;
            Stats.FragmentationRatio = static_cast<double>(Stats.PageHeapFreeBytes) / UsedBytes;
        }
    }
#else
    // 标准库模式下使用基础统计
    auto BasicStats = GetStats();
    Stats.HeapSize = BasicStats.CurrentUsage;
    Stats.FragmentationRatio = 0.1;  // 假设 10% 碎片率
#endif

    return Stats;
}
}  // namespace Helianthus::Common

// 全局 new/delete 操作符实现
void* operator new(std::size_t Size)
{
    return Helianthus::Common::TCMallocWrapper::New(Size);
}

void* operator new[](std::size_t Size)
{
    return Helianthus::Common::TCMallocWrapper::New(Size);
}

void* operator new(std::size_t Size, const std::nothrow_t&) noexcept
{
    return Helianthus::Common::TCMallocWrapper::New(Size);
}

void* operator new[](std::size_t Size, const std::nothrow_t&) noexcept
{
    return Helianthus::Common::TCMallocWrapper::New(Size);
}

void operator delete(void* Ptr) noexcept
{
    Helianthus::Common::TCMallocWrapper::Delete(Ptr);
}

void operator delete[](void* Ptr) noexcept
{
    Helianthus::Common::TCMallocWrapper::DeleteArray(Ptr);
}

void operator delete(void* Ptr, const std::nothrow_t&) noexcept
{
    Helianthus::Common::TCMallocWrapper::Delete(Ptr);
}

void operator delete[](void* Ptr, const std::nothrow_t&) noexcept
{
    Helianthus::Common::TCMallocWrapper::DeleteArray(Ptr);
}

// C++17 对齐分配支持
void* operator new(std::size_t Size, std::align_val_t Alignment)
{
    return Helianthus::Common::TCMallocWrapper::AlignedMalloc(Size, static_cast<size_t>(Alignment));
}

void* operator new[](std::size_t Size, std::align_val_t Alignment)
{
    return Helianthus::Common::TCMallocWrapper::AlignedMalloc(Size, static_cast<size_t>(Alignment));
}

void* operator new(std::size_t Size, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    return Helianthus::Common::TCMallocWrapper::AlignedMalloc(Size, static_cast<size_t>(Alignment));
}

void* operator new[](std::size_t Size, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    return Helianthus::Common::TCMallocWrapper::AlignedMalloc(Size, static_cast<size_t>(Alignment));
}

void operator delete(void* Ptr, std::align_val_t Alignment) noexcept
{
    Helianthus::Common::TCMallocWrapper::AlignedFree(Ptr);
}

void operator delete[](void* Ptr, std::align_val_t Alignment) noexcept
{
    Helianthus::Common::TCMallocWrapper::AlignedFree(Ptr);
}

void operator delete(void* Ptr, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    Helianthus::Common::TCMallocWrapper::AlignedFree(Ptr);
}

void operator delete[](void* Ptr, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    Helianthus::Common::TCMallocWrapper::AlignedFree(Ptr);
}
