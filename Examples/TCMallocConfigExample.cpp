#include "Shared/Common/TCMallocWrapper.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace Helianthus::Common;

// 演示如何使用 TCMalloc 运行时配置
void DemoBasicConfig()
{
    std::cout << "=== TCMalloc 基础配置演示 ===" << std::endl;

    // 初始化 TCMalloc
    TCMALLOC_INIT();

    // 获取默认配置
    auto DefaultConfig = TCMALLOC_GET_CONFIG();
    std::cout << "默认配置:" << std::endl;
    std::cout << "  最大总线程缓存: " << DefaultConfig.MaxTotalThreadCacheBytes / (1024 * 1024)
              << " MB" << std::endl;
    std::cout << "  最大单线程缓存: " << DefaultConfig.MaxThreadCacheBytes / (1024 * 1024) << " MB"
              << std::endl;
    std::cout << "  页堆空闲字节: " << DefaultConfig.PageHeapFreeBytes / (1024 * 1024) << " MB"
              << std::endl;
    std::cout << "  采样率: " << DefaultConfig.SampleRate / (1024 * 1024) << " MB" << std::endl;
    std::cout << "  启用采样: " << (DefaultConfig.EnableSampling ? "是" : "否") << std::endl;

    // 创建自定义配置
    TCMallocWrapper::RuntimeConfig CustomConfig;
    CustomConfig.MaxTotalThreadCacheBytes = 128 * 1024 * 1024;  // 128MB
    CustomConfig.MaxThreadCacheBytes = 8 * 1024 * 1024;         // 8MB
    CustomConfig.EnableSampling = true;
    CustomConfig.SampleRate = 2 * 1024 * 1024;  // 2MB
    CustomConfig.EnableDetailedStats = true;

    // 应用配置
    bool Result = TCMALLOC_SET_CONFIG(CustomConfig);
    std::cout << "配置应用结果: " << (Result ? "成功" : "失败") << std::endl;

    // 验证配置
    auto NewConfig = TCMALLOC_GET_CONFIG();
    std::cout << "新配置:" << std::endl;
    std::cout << "  最大总线程缓存: " << NewConfig.MaxTotalThreadCacheBytes / (1024 * 1024) << " MB"
              << std::endl;
    std::cout << "  最大单线程缓存: " << NewConfig.MaxThreadCacheBytes / (1024 * 1024) << " MB"
              << std::endl;
    std::cout << "  启用采样: " << (NewConfig.EnableSampling ? "是" : "否") << std::endl;
    std::cout << "  采样率: " << NewConfig.SampleRate / (1024 * 1024) << " MB" << std::endl;
}

// 演示性能优化配置
void DemoPerformanceConfig()
{
    std::cout << "\n=== TCMalloc 性能优化配置演示 ===" << std::endl;

    // 设置性能优化配置
    bool Result = TCMallocWrapper::SetPerformanceConfig(true,      // 启用激进内存释放
                                                        true,      // 启用大内存分配优化
                                                        64 * 1024  // 64KB 大内存阈值
    );

    std::cout << "性能配置设置结果: " << (Result ? "成功" : "失败") << std::endl;

    // 设置线程缓存配置
    Result = TCMallocWrapper::SetThreadCacheConfig(256 * 1024 * 1024,  // 256MB 总缓存
                                                   16 * 1024 * 1024,   // 16MB 单线程缓存
                                                   8 * 1024 * 1024     // 8MB 缓存大小
    );

    std::cout << "线程缓存配置设置结果: " << (Result ? "成功" : "失败") << std::endl;

    // 设置页堆配置
    Result = TCMallocWrapper::SetPageHeapConfig(512 * 1024 * 1024,  // 512MB 页堆空闲
                                                256 * 1024 * 1024   // 256MB 页堆取消映射
    );

    std::cout << "页堆配置设置结果: " << (Result ? "成功" : "失败") << std::endl;
}

// 演示内存分配和统计
void DemoAllocationAndStats()
{
    std::cout << "\n=== TCMalloc 内存分配和统计演示 ===" << std::endl;

    // 重置统计
    TCMALLOC_RESET_STATS();

    // 分配一些内存
    std::vector<void*> Allocations;
    const size_t AllocSize = 1024;
    const int NumAllocs = 1000;

    std::cout << "分配 " << NumAllocs << " 个 " << AllocSize << " 字节的内存块..." << std::endl;

    auto StartTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NumAllocs; ++i)
    {
        void* Ptr = TCMallocWrapper::Malloc(AllocSize);
        if (Ptr)
        {
            Allocations.push_back(Ptr);
        }
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

    std::cout << "分配耗时: " << Duration.count() << " 微秒" << std::endl;

    // 获取基础统计
    auto Stats = TCMALLOC_STATS();
    std::cout << "基础统计:" << std::endl;
    std::cout << "  总分配: " << Stats.TotalAllocated / 1024 << " KB" << std::endl;
    std::cout << "  总释放: " << Stats.TotalFreed / 1024 << " KB" << std::endl;
    std::cout << "  当前使用: " << Stats.CurrentUsage / 1024 << " KB" << std::endl;
    std::cout << "  峰值使用: " << Stats.PeakUsage / 1024 << " KB" << std::endl;
    std::cout << "  分配块数: " << Stats.AllocatedBlocks << std::endl;
    std::cout << "  释放块数: " << Stats.FreedBlocks << std::endl;

    // 获取高级统计
    auto AdvStats = TCMALLOC_ADVANCED_STATS();
    std::cout << "高级统计:" << std::endl;
    std::cout << "  堆大小: " << AdvStats.HeapSize / 1024 << " KB" << std::endl;
    std::cout << "  页堆空闲: " << AdvStats.PageHeapFreeBytes / 1024 << " KB" << std::endl;
    std::cout << "  线程缓存: " << AdvStats.TotalThreadCacheBytes / 1024 << " KB" << std::endl;
    std::cout << "  中央缓存: " << AdvStats.CentralCacheBytes / 1024 << " KB" << std::endl;
    std::cout << "  碎片率: " << (AdvStats.FragmentationRatio * 100) << "%" << std::endl;

    // 释放内存
    std::cout << "释放内存..." << std::endl;
    StartTime = std::chrono::high_resolution_clock::now();

    for (void* Ptr : Allocations)
    {
        TCMallocWrapper::Free(Ptr);
    }

    EndTime = std::chrono::high_resolution_clock::now();
    Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

    std::cout << "释放耗时: " << Duration.count() << " 微秒" << std::endl;

    // 强制垃圾回收
    TCMALLOC_FORCE_GC();

    // 获取释放后的统计
    Stats = TCMALLOC_STATS();
    std::cout << "释放后统计:" << std::endl;
    std::cout << "  当前使用: " << Stats.CurrentUsage / 1024 << " KB" << std::endl;
    std::cout << "  释放块数: " << Stats.FreedBlocks << std::endl;
}

// 演示调试配置
void DemoDebugConfig()
{
    std::cout << "\n=== TCMalloc 调试配置演示 ===" << std::endl;

    // 启用调试模式
    bool Result = TCMallocWrapper::SetDebugConfig(true,  // 启用调试模式
                                                  true,  // 启用内存泄漏检查
                                                  10     // 栈深度为 10
    );

    std::cout << "调试配置设置结果: " << (Result ? "成功" : "失败") << std::endl;

    // 启用采样
    Result = TCMallocWrapper::SetSamplingConfig(512 * 1024,  // 512KB 采样率
                                                true         // 启用采样
    );

    std::cout << "采样配置设置结果: " << (Result ? "成功" : "失败") << std::endl;

    // 分配一些内存来触发采样
    std::vector<void*> TestAllocs;
    for (int i = 0; i < 100; ++i)
    {
        void* Ptr = TCMallocWrapper::Malloc(1024 * (i + 1));  // 递增大小
        if (Ptr)
        {
            TestAllocs.push_back(Ptr);
        }
    }

    // 获取线程缓存统计
    auto ThreadStats = TCMallocWrapper::GetThreadCacheStats();
    std::cout << "线程缓存统计:" << std::endl;
    std::cout << "  缓存大小: " << ThreadStats.CacheSize / 1024 << " KB" << std::endl;
    std::cout << "  缓存命中率: " << ThreadStats.CacheHitRate << "%" << std::endl;
    std::cout << "  分配字节: " << ThreadStats.AllocatedBytes / 1024 << " KB" << std::endl;
    std::cout << "  释放字节: " << ThreadStats.FreedBytes / 1024 << " KB" << std::endl;

    // 释放测试内存
    for (void* Ptr : TestAllocs)
    {
        TCMallocWrapper::Free(Ptr);
    }

    // 检查是否有内存泄漏
    bool HasLeaks = TCMallocWrapper::IsMemoryLeakDetected();
    std::cout << "检测到内存泄漏: " << (HasLeaks ? "是" : "否") << std::endl;

    if (HasLeaks)
    {
        std::cout << "转储内存泄漏信息..." << std::endl;
        TCMallocWrapper::DumpMemoryLeaks();
    }
}

// 演示运行时操作
void DemoRuntimeOperations()
{
    std::cout << "\n=== TCMalloc 运行时操作演示 ===" << std::endl;

    // 分配一些内存
    std::vector<void*> BigAllocs;
    for (int i = 0; i < 50; ++i)
    {
        void* Ptr = TCMallocWrapper::Malloc(64 * 1024);  // 64KB
        if (Ptr)
        {
            BigAllocs.push_back(Ptr);
        }
    }

    std::cout << "分配了 50 个 64KB 的大内存块" << std::endl;

    // 获取分配前的统计
    auto StatsBefore = TCMALLOC_ADVANCED_STATS();
    std::cout << "操作前堆大小: " << StatsBefore.HeapSize / 1024 << " KB" << std::endl;

    // 释放一半内存
    for (size_t i = 0; i < BigAllocs.size() / 2; ++i)
    {
        TCMallocWrapper::Free(BigAllocs[i]);
    }
    BigAllocs.erase(BigAllocs.begin(), BigAllocs.begin() + BigAllocs.size() / 2);

    std::cout << "释放了 25 个内存块" << std::endl;

    // 刷新线程缓存
    std::cout << "刷新线程缓存..." << std::endl;
    TCMALLOC_FLUSH_CACHES();

    // 释放内存到系统
    std::cout << "释放内存到系统..." << std::endl;
    TCMALLOC_RELEASE_MEMORY();

    // 强制垃圾回收
    std::cout << "强制垃圾回收..." << std::endl;
    TCMALLOC_FORCE_GC();

    // 获取操作后的统计
    auto StatsAfter = TCMALLOC_ADVANCED_STATS();
    std::cout << "操作后堆大小: " << StatsAfter.HeapSize / 1024 << " KB" << std::endl;
    std::cout << "释放的内存: " << (StatsBefore.HeapSize - StatsAfter.HeapSize) / 1024 << " KB"
              << std::endl;

    // 释放剩余内存
    for (void* Ptr : BigAllocs)
    {
        TCMallocWrapper::Free(Ptr);
    }
}

int main()
{
    std::cout << "TCMalloc 运行时配置示例程序" << std::endl;
    std::cout << "========================================" << std::endl;

    try
    {
        // 基础配置演示
        DemoBasicConfig();

        // 性能配置演示
        DemoPerformanceConfig();

        // 内存分配和统计演示
        DemoAllocationAndStats();

        // 调试配置演示
        DemoDebugConfig();

        // 运行时操作演示
        DemoRuntimeOperations();

        std::cout << "\n========================================" << std::endl;
        std::cout << "所有演示完成！" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    // 清理
    TCMALLOC_SHUTDOWN();

    return 0;
}
