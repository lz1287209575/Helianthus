#include <gtest/gtest.h>
#include "Shared/Common/TCMallocWrapper.h"

using namespace Helianthus::Common;

class TCMallocRuntimeConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        TCMALLOC_INIT();
    }

    void TearDown() override
    {
        TCMALLOC_SHUTDOWN();
    }
};

TEST_F(TCMallocRuntimeConfigTest, DefaultConfig)
{
    // 测试默认配置获取
    auto Config = TCMALLOC_GET_CONFIG();
    
    EXPECT_EQ(Config.MaxTotalThreadCacheBytes, 64 * 1024 * 1024);  // 64MB
    EXPECT_EQ(Config.MaxThreadCacheBytes, 4 * 1024 * 1024);       // 4MB
    EXPECT_EQ(Config.ThreadCacheSize, 2 * 1024 * 1024);           // 2MB
    EXPECT_EQ(Config.PageHeapFreeBytes, 256 * 1024 * 1024);       // 256MB
    EXPECT_EQ(Config.PageHeapUnmapBytes, 128 * 1024 * 1024);      // 128MB
    EXPECT_EQ(Config.SampleRate, 1024 * 1024);                    // 1MB
    EXPECT_FALSE(Config.EnableSampling);
    EXPECT_FALSE(Config.EnableAggressiveDecommit);
    EXPECT_TRUE(Config.EnableLargeAllocs);
    EXPECT_EQ(Config.LargeAllocThreshold, 32 * 1024);             // 32KB
    EXPECT_FALSE(Config.EnableDebugMode);
    EXPECT_FALSE(Config.EnableMemoryLeakCheck);
    EXPECT_EQ(Config.DebugAllocStackDepth, 0);
    EXPECT_FALSE(Config.EnableDetailedStats);
    EXPECT_FALSE(Config.EnablePerThreadStats);
    EXPECT_EQ(Config.GCThreshold, 128 * 1024 * 1024);             // 128MB
    EXPECT_FALSE(Config.EnablePeriodicGC);
    EXPECT_EQ(Config.GCIntervalMs, 30000);                        // 30秒
}

TEST_F(TCMallocRuntimeConfigTest, SetRuntimeConfig)
{
    // 创建自定义配置
    TCMallocWrapper::RuntimeConfig CustomConfig;
    CustomConfig.MaxTotalThreadCacheBytes = 128 * 1024 * 1024;   // 128MB
    CustomConfig.MaxThreadCacheBytes = 8 * 1024 * 1024;          // 8MB
    CustomConfig.ThreadCacheSize = 4 * 1024 * 1024;              // 4MB
    CustomConfig.PageHeapFreeBytes = 512 * 1024 * 1024;          // 512MB
    CustomConfig.PageHeapUnmapBytes = 256 * 1024 * 1024;         // 256MB
    CustomConfig.SampleRate = 2 * 1024 * 1024;                   // 2MB
    CustomConfig.EnableSampling = true;
    CustomConfig.EnableAggressiveDecommit = true;
    CustomConfig.EnableLargeAllocs = true;
    CustomConfig.LargeAllocThreshold = 64 * 1024;                // 64KB
    CustomConfig.EnableDebugMode = true;
    CustomConfig.EnableMemoryLeakCheck = true;
    CustomConfig.DebugAllocStackDepth = 10;
    CustomConfig.EnableDetailedStats = true;
    CustomConfig.EnablePerThreadStats = true;
    CustomConfig.GCThreshold = 256 * 1024 * 1024;                // 256MB
    CustomConfig.EnablePeriodicGC = true;
    CustomConfig.GCIntervalMs = 60000;                           // 60秒
    
    // 设置配置
    bool Result = TCMALLOC_SET_CONFIG(CustomConfig);
    EXPECT_TRUE(Result);
    
    // 验证配置已设置
    auto RetrievedConfig = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(RetrievedConfig.MaxTotalThreadCacheBytes, CustomConfig.MaxTotalThreadCacheBytes);
    EXPECT_EQ(RetrievedConfig.MaxThreadCacheBytes, CustomConfig.MaxThreadCacheBytes);
    EXPECT_EQ(RetrievedConfig.ThreadCacheSize, CustomConfig.ThreadCacheSize);
    EXPECT_EQ(RetrievedConfig.PageHeapFreeBytes, CustomConfig.PageHeapFreeBytes);
    EXPECT_EQ(RetrievedConfig.PageHeapUnmapBytes, CustomConfig.PageHeapUnmapBytes);
    EXPECT_EQ(RetrievedConfig.SampleRate, CustomConfig.SampleRate);
    EXPECT_EQ(RetrievedConfig.EnableSampling, CustomConfig.EnableSampling);
    EXPECT_EQ(RetrievedConfig.EnableAggressiveDecommit, CustomConfig.EnableAggressiveDecommit);
    EXPECT_EQ(RetrievedConfig.EnableLargeAllocs, CustomConfig.EnableLargeAllocs);
    EXPECT_EQ(RetrievedConfig.LargeAllocThreshold, CustomConfig.LargeAllocThreshold);
    EXPECT_EQ(RetrievedConfig.EnableDebugMode, CustomConfig.EnableDebugMode);
    EXPECT_EQ(RetrievedConfig.EnableMemoryLeakCheck, CustomConfig.EnableMemoryLeakCheck);
    EXPECT_EQ(RetrievedConfig.DebugAllocStackDepth, CustomConfig.DebugAllocStackDepth);
    EXPECT_EQ(RetrievedConfig.EnableDetailedStats, CustomConfig.EnableDetailedStats);
    EXPECT_EQ(RetrievedConfig.EnablePerThreadStats, CustomConfig.EnablePerThreadStats);
    EXPECT_EQ(RetrievedConfig.GCThreshold, CustomConfig.GCThreshold);
    EXPECT_EQ(RetrievedConfig.EnablePeriodicGC, CustomConfig.EnablePeriodicGC);
    EXPECT_EQ(RetrievedConfig.GCIntervalMs, CustomConfig.GCIntervalMs);
}

TEST_F(TCMallocRuntimeConfigTest, ThreadCacheConfig)
{
    // 测试线程缓存配置
    size_t MaxTotal = 256 * 1024 * 1024;     // 256MB
    size_t MaxPerThread = 16 * 1024 * 1024;  // 16MB
    size_t CacheSize = 8 * 1024 * 1024;      // 8MB
    
    bool Result = TCMallocWrapper::SetThreadCacheConfig(MaxTotal, MaxPerThread, CacheSize);
    EXPECT_TRUE(Result);
    
    auto Config = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(Config.MaxTotalThreadCacheBytes, MaxTotal);
    EXPECT_EQ(Config.MaxThreadCacheBytes, MaxPerThread);
    EXPECT_EQ(Config.ThreadCacheSize, CacheSize);
}

TEST_F(TCMallocRuntimeConfigTest, PageHeapConfig)
{
    // 测试页堆配置
    size_t FreeBytes = 1024 * 1024 * 1024;   // 1GB
    size_t UnmapBytes = 512 * 1024 * 1024;   // 512MB
    
    bool Result = TCMallocWrapper::SetPageHeapConfig(FreeBytes, UnmapBytes);
    EXPECT_TRUE(Result);
    
    auto Config = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(Config.PageHeapFreeBytes, FreeBytes);
    EXPECT_EQ(Config.PageHeapUnmapBytes, UnmapBytes);
}

TEST_F(TCMallocRuntimeConfigTest, SamplingConfig)
{
    // 测试采样配置
    size_t SampleRate = 4 * 1024 * 1024;     // 4MB
    bool EnableSampling = true;
    
    bool Result = TCMallocWrapper::SetSamplingConfig(SampleRate, EnableSampling);
    EXPECT_TRUE(Result);
    
    auto Config = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(Config.SampleRate, SampleRate);
    EXPECT_EQ(Config.EnableSampling, EnableSampling);
    
    // 测试禁用采样
    Result = TCMallocWrapper::SetSamplingConfig(0, false);
    EXPECT_TRUE(Result);
    
    Config = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(Config.SampleRate, 0);
    EXPECT_FALSE(Config.EnableSampling);
}

TEST_F(TCMallocRuntimeConfigTest, PerformanceConfig)
{
    // 测试性能配置
    bool AggressiveDecommit = true;
    bool LargeAllocs = true;
    size_t LargeThreshold = 128 * 1024;      // 128KB
    
    bool Result = TCMallocWrapper::SetPerformanceConfig(AggressiveDecommit, LargeAllocs, LargeThreshold);
    EXPECT_TRUE(Result);
    
    auto Config = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(Config.EnableAggressiveDecommit, AggressiveDecommit);
    EXPECT_EQ(Config.EnableLargeAllocs, LargeAllocs);
    EXPECT_EQ(Config.LargeAllocThreshold, LargeThreshold);
}

TEST_F(TCMallocRuntimeConfigTest, DebugConfig)
{
    // 测试调试配置
    bool DebugMode = true;
    bool LeakCheck = true;
    size_t StackDepth = 20;
    
    bool Result = TCMallocWrapper::SetDebugConfig(DebugMode, LeakCheck, StackDepth);
    EXPECT_TRUE(Result);
    
    auto Config = TCMALLOC_GET_CONFIG();
    EXPECT_EQ(Config.EnableDebugMode, DebugMode);
    EXPECT_EQ(Config.EnableMemoryLeakCheck, LeakCheck);
    EXPECT_EQ(Config.DebugAllocStackDepth, StackDepth);
}

TEST_F(TCMallocRuntimeConfigTest, RuntimeOperations)
{
    // 测试运行时操作（这些操作不会崩溃即可）
    EXPECT_NO_THROW(TCMALLOC_FORCE_GC());
    EXPECT_NO_THROW(TCMALLOC_RELEASE_MEMORY());
    EXPECT_NO_THROW(TCMALLOC_FLUSH_CACHES());
}

TEST_F(TCMallocRuntimeConfigTest, AdvancedStats)
{
    // 测试高级统计
    auto Stats = TCMALLOC_ADVANCED_STATS();
    
    // 基本验证统计数据结构正确
    EXPECT_GE(Stats.HeapSize, 0);
    EXPECT_GE(Stats.UnmappedBytes, 0);
    EXPECT_GE(Stats.PageHeapFreeBytes, 0);
    EXPECT_GE(Stats.PageHeapUnmappedBytes, 0);
    EXPECT_GE(Stats.TotalThreadCacheBytes, 0);
    EXPECT_GE(Stats.CentralCacheBytes, 0);
    EXPECT_GE(Stats.TransferCacheBytes, 0);
    EXPECT_GE(Stats.SpanCacheBytes, 0);
    EXPECT_GE(Stats.SampledObjects, 0);
    EXPECT_GE(Stats.FragmentationRatio, 0.0);
    EXPECT_LE(Stats.FragmentationRatio, 1.0);
}

TEST_F(TCMallocRuntimeConfigTest, AllocationWithConfig)
{
    // 测试在配置更改后的内存分配
    TCMallocWrapper::RuntimeConfig CustomConfig;
    CustomConfig.MaxTotalThreadCacheBytes = 32 * 1024 * 1024;    // 32MB
    CustomConfig.MaxThreadCacheBytes = 2 * 1024 * 1024;          // 2MB
    
    bool ConfigResult = TCMALLOC_SET_CONFIG(CustomConfig);
    EXPECT_TRUE(ConfigResult);
    
    // 分配一些内存
    std::vector<void*> Allocations;
    for (int i = 0; i < 100; ++i)
    {
        void* Ptr = TCMallocWrapper::Malloc(1024);
        EXPECT_NE(Ptr, nullptr);
        Allocations.push_back(Ptr);
    }
    
    // 检查统计
    auto Stats = TCMALLOC_STATS();
    EXPECT_GT(Stats.AllocatedBlocks, 0);
    EXPECT_GT(Stats.TotalAllocated, 0);
    
    // 释放内存
    for (void* Ptr : Allocations)
    {
        TCMallocWrapper::Free(Ptr);
    }
    
    // 强制垃圾回收
    TCMALLOC_FORCE_GC();
    
    // 检查高级统计
    auto AdvStats = TCMALLOC_ADVANCED_STATS();
    EXPECT_GE(AdvStats.HeapSize, 0);
}
