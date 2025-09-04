#include "Shared/Common/LogCategories.h"
#include "Shared/MessageQueue/MessageQueue.h"

#include <algorithm>

namespace Helianthus::MessageQueue
{

void MessageQueue::InitializeMemoryPool()
{
    std::lock_guard<std::mutex> guard(MemoryPoolMutex);
    if (MemoryPoolData != nullptr)
        return;
    MemoryPoolSize = MemoryPoolConfigData.InitialSize;
    MemoryPoolData = std::malloc(MemoryPoolSize);
    if (!MemoryPoolData)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "内存池初始化失败: 分配 {} 字节失败",
              MemoryPoolSize);
        return;
    }
    const size_t blockSize = std::max<size_t>(MemoryPoolConfigData.BlockSize, 64);
    const size_t blockCount = MemoryPoolSize / blockSize;
    MemoryPoolBlocks.reserve(blockCount);
    FreeBlocks.reserve(blockCount);
    char* base = static_cast<char*>(MemoryPoolData);
    for (size_t i = 0; i < blockCount; ++i)
    {
        auto* block = new MemoryBlock();
        block->Data = base + i * blockSize;
        block->Size = blockSize;
        block->IsUsed = false;
        block->Next = nullptr;
        block->AllocTime = 0;
        MemoryPoolBlocks.push_back(block);
        FreeBlocks.push_back(block);
    }
    MemoryPoolUsed = 0;
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "内存池初始化完成: size={} bytes, block_size={}, blocks={}",
          MemoryPoolSize,
          blockSize,
          blockCount);
}

void MessageQueue::CleanupMemoryPool()
{
    std::lock_guard<std::mutex> guard(MemoryPoolMutex);
    for (auto* block : MemoryPoolBlocks)
        delete block;
    MemoryPoolBlocks.clear();
    FreeBlocks.clear();
    if (MemoryPoolData)
    {
        std::free(MemoryPoolData);
        MemoryPoolData = nullptr;
    }
    MemoryPoolSize = 0;
    MemoryPoolUsed = 0;
}

MemoryBlock* MessageQueue::FindFreeBlock(size_t Size)
{
    for (auto* block : FreeBlocks)
    {
        if (!block->IsUsed && block->Size >= Size)
            return block;
    }
    return nullptr;
}

void MessageQueue::MarkBlockAsUsed(MemoryBlock* Block)
{
    if (Block == nullptr || Block->IsUsed)
        return;
    Block->IsUsed = true;
    Block->AllocTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    auto it = std::find(FreeBlocks.begin(), FreeBlocks.end(), Block);
    if (it != FreeBlocks.end())
        FreeBlocks.erase(it);
    MemoryPoolUsed += Block->Size;
}

void MessageQueue::MarkBlockAsFree(MemoryBlock* Block)
{
    if (Block == nullptr || !Block->IsUsed)
        return;
    Block->IsUsed = false;
    Block->AllocTime = 0;
    FreeBlocks.push_back(Block);
    if (MemoryPoolUsed >= Block->Size)
        MemoryPoolUsed -= Block->Size;
}

QueueResult MessageQueue::SetMemoryPoolConfig(const MemoryPoolConfig& Config)
{
    {
        std::unique_lock<std::shared_mutex> lock(MemoryPoolConfigMutex);
        MemoryPoolConfigData = Config;
    }
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "设置内存池配置: initial_size={}, max_size={}, block_size={}",
          Config.InitialSize,
          Config.MaxSize,
          Config.BlockSize);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetMemoryPoolConfig(MemoryPoolConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> lock(MemoryPoolConfigMutex);
    OutConfig = MemoryPoolConfigData;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SetBufferConfig(const BufferConfig& Config)
{
    {
        std::unique_lock<std::shared_mutex> lock(BufferConfigMutex);
        BufferConfigData = Config;
    }
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "设置缓冲区配置: initial_capacity={}, max_capacity={}, batching={} size={} timeout_ms={} "
          "zero_copy={}",
          Config.InitialCapacity,
          Config.MaxCapacity,
          Config.EnableBatching,
          Config.BatchSize,
          Config.BatchTimeoutMs,
          Config.EnableZeroCopy);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetBufferConfig(BufferConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> lock(BufferConfigMutex);
    OutConfig = BufferConfigData;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetPerformanceStats(PerformanceStats& OutStats) const
{
    std::lock_guard<std::mutex> guard(PerformanceStatsMutex);
    OutStats = PerformanceStatsData;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ResetPerformanceStats()
{
    {
        std::lock_guard<std::mutex> guard(PerformanceStatsMutex);
        PerformanceStatsData = PerformanceStats{};
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "重置性能统计");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::AllocateFromPool(size_t Size, void*& OutPtr)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    std::lock_guard<std::mutex> guard(MemoryPoolMutex);
    OutPtr = nullptr;
    MemoryBlock* block = FindFreeBlock(Size);
    if (block)
    {
        MarkBlockAsUsed(block);
        OutPtr = block->Data;
        {
            std::lock_guard<std::mutex> s(PerformanceStatsMutex);
            PerformanceStatsData.MemoryPoolHits++;
        }
    }
    else
    {
        OutPtr = std::malloc(Size);
        {
            std::lock_guard<std::mutex> s(PerformanceStatsMutex);
            PerformanceStatsData.MemoryPoolMisses++;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("allocation", ms, Size);
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Verbose,
          "从内存池分配: size={}, ptr={}, hit={}",
          Size,
          OutPtr,
          block != nullptr);
    return OutPtr ? QueueResult::SUCCESS : QueueResult::OPERATION_FAILED;
}

QueueResult MessageQueue::DeallocateToPool(void* Ptr, size_t Size)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    bool returnedToPool = false;
    {
        std::lock_guard<std::mutex> guard(MemoryPoolMutex);
        for (auto* block : MemoryPoolBlocks)
        {
            if (block->Data == Ptr)
            {
                MarkBlockAsFree(block);
                returnedToPool = true;
                break;
            }
        }
    }
    if (!returnedToPool)
        std::free(Ptr);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("deallocation", ms, Size);
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Verbose,
          "释放到{}: ptr={}, size={}",
          returnedToPool ? "内存池" : "系统",
          Ptr,
          Size);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CompactMemoryPool()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "内存池压缩");
    return QueueResult::SUCCESS;
}

void MessageQueue::UpdatePerformanceStats(const std::string& Operation, double TimeMs, size_t Size)
{
    std::lock_guard<std::mutex> guard(PerformanceStatsMutex);
    auto& s = PerformanceStatsData;
    if (Operation == "allocation")
    {
        s.TotalAllocations++;
        s.TotalBytesAllocated += Size;
        s.CurrentBytesAllocated += Size;
        s.PeakBytesAllocated = std::max(s.PeakBytesAllocated, s.CurrentBytesAllocated);
        s.AverageAllocationTimeMs = (s.AverageAllocationTimeMs + TimeMs) / 2.0;
    }
    else if (Operation == "deallocation")
    {
        s.TotalDeallocations++;
        if (s.CurrentBytesAllocated >= Size)
            s.CurrentBytesAllocated -= Size;
        s.AverageDeallocationTimeMs = (s.AverageDeallocationTimeMs + TimeMs) / 2.0;
    }
    else if (Operation == "zero_copy")
    {
        s.ZeroCopyOperations++;
        s.AverageZeroCopyTimeMs = (s.AverageZeroCopyTimeMs + TimeMs) / 2.0;
    }
    else if (Operation == "batch")
    {
        s.BatchOperations++;
        s.AverageBatchTimeMs = (s.AverageBatchTimeMs + TimeMs) / 2.0;
    }
    const uint64_t totalReq = s.MemoryPoolHits + s.MemoryPoolMisses;
    if (totalReq > 0)
        s.MemoryPoolHitRate = static_cast<double>(s.MemoryPoolHits) / static_cast<double>(totalReq);
    s.LastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
}

}  // namespace Helianthus::MessageQueue
