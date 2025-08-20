#include "Shared/Common/TCMallocWrapper.h"
#include <cstring>
#include <atomic>
#include <mutex>
#include <cstdlib>

// 条件编译：如果有 TCMalloc 头文件，就使用它
#ifdef HELIANTHUS_USE_TCMALLOC
    // 当真正的 TCMalloc 可用时，取消这些注释
    // #include <gperftools/tcmalloc.h>
    // #include <gperftools/malloc_extension.h>
    // #include <gperftools/profiler.h>
    #define USE_REAL_TCMALLOC 0  // 暂时设为 0，等 gperftools 构建成功后改为 1
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

    // TCMallocWrapper 实现
    bool TCMallocWrapper::Initialize()
    {
        std::lock_guard<std::mutex> lock(GInitMutex);
        
        if (GInitialized.load()) {
            return true;
        }
        
        // 暂时使用标准 malloc，后续可以替换为 TCMalloc
        // 当真正的 TCMalloc 可用时，可以在这里初始化它
        
#if USE_REAL_TCMALLOC
        // 初始化真正的 TCMalloc
        // MallocExtension::instance()->SetNumericProperty("tcmalloc.max_total_thread_cache_bytes", 64 * 1024 * 1024);
        // MallocExtension::instance()->SetNumericProperty("tcmalloc.page_heap_free_bytes", 256 * 1024 * 1024);
#endif
        
        GInitialized.store(true);
        return true;
    }

    void TCMallocWrapper::Shutdown()
    {
        std::lock_guard<std::mutex> lock(GInitMutex);
        
        if (!GInitialized.load()) {
            return;
        }
        
        // 输出最终统计信息
        auto Stats = GetStats();
        if (Stats.CurrentUsage > 0) {
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
        if (!GInitialized.load()) {
            Initialize();
        }
        
        void* Ptr = nullptr;
        
#if USE_REAL_TCMALLOC
        // 使用真正的 TCMalloc
        // Ptr = tc_malloc(Size);
        Ptr = malloc(Size);  // 暂时回退到标准库
#else
        // 使用标准库
        Ptr = malloc(Size);
#endif
        
        if (Ptr) {
            GTotalAllocated.fetch_add(Size);
            GAllocatedBlocks.fetch_add(1);
            
            size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
            size_t PeakUsage = GPeakUsage.load();
            while (CurrentUsage > PeakUsage && !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage)) {
                // 重试直到成功更新峰值
            }
        }
        
        return Ptr;
    }

    void* TCMallocWrapper::Calloc(size_t Count, size_t Size)
    {
        if (!GInitialized.load()) {
            Initialize();
        }
        
        void* Ptr = calloc(Count, Size);
        if (Ptr) {
            size_t TotalSize = Count * Size;
            GTotalAllocated.fetch_add(TotalSize);
            GAllocatedBlocks.fetch_add(1);
            
            size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
            size_t PeakUsage = GPeakUsage.load();
            while (CurrentUsage > PeakUsage && !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage)) {
                // 重试直到成功更新峰值
            }
        }
        
        return Ptr;
    }

    void* TCMallocWrapper::Realloc(void* Ptr, size_t NewSize)
    {
        if (!GInitialized.load()) {
            Initialize();
        }
        
        // 注意：这里我们无法准确获取旧大小，所以统计可能不准确
        void* NewPtr = realloc(Ptr, NewSize);
        if (NewPtr) {
            // 简化处理：假设重新分配成功
            GTotalAllocated.fetch_add(NewSize);
            GAllocatedBlocks.fetch_add(1);
            
            size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
            size_t PeakUsage = GPeakUsage.load();
            while (CurrentUsage > PeakUsage && !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage)) {
                // 重试直到成功更新峰值
            }
        }
        
        return NewPtr;
    }

    void TCMallocWrapper::Free(void* Ptr)
    {
        if (!Ptr) {
            return;
        }
        
        size_t Size = 0;
        
#if USE_REAL_TCMALLOC
        // 使用真正的 TCMalloc
        // Size = tc_malloc_size(Ptr);
        // tc_free(Ptr);
        Size = 1024;  // 暂时使用估算值
        free(Ptr);    // 暂时回退到标准库
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
        if (!GInitialized.load()) {
            Initialize();
        }
        
        void* Ptr = aligned_alloc(Alignment, Size);
        if (Ptr) {
            GTotalAllocated.fetch_add(Size);
            GAllocatedBlocks.fetch_add(1);
            
            size_t CurrentUsage = GTotalAllocated.load() - GTotalFreed.load();
            size_t PeakUsage = GPeakUsage.load();
            while (CurrentUsage > PeakUsage && !GPeakUsage.compare_exchange_weak(PeakUsage, CurrentUsage)) {
                // 重试直到成功更新峰值
            }
        }
        
        return Ptr;
    }

    void TCMallocWrapper::AlignedFree(void* Ptr)
    {
        Free(Ptr);
    }

    TCMallocWrapper::MemoryStats TCMallocWrapper::GetStats()
    {
        MemoryStats Stats;
        
        if (GInitialized.load()) {
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
        // 暂时返回默认值
        return Stats;
    }
}

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
