#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Common/Logger.h"
#include "MessageTypes.h"

namespace Helianthus::MessageQueue
{

// 性能优化配置
struct PerformanceConfig
{
    // 内存池配置
    size_t MemoryPoolSize = 64 * 1024 * 1024;  // 64MB
    size_t BlockSize = 4096;                   // 4KB块
    size_t MaxPoolSize = 512 * 1024 * 1024;    // 512MB
    bool EnableMemoryPool = true;

    // 对象池配置
    size_t MessagePoolSize = 10000;      // 消息对象池大小
    size_t MessagePoolMaxSize = 100000;  // 最大消息对象池大小
    bool EnableMessagePool = true;

    // 批处理配置
    uint32_t BatchSize = 100;       // 批处理大小
    uint32_t BatchTimeoutMs = 100;  // 批处理超时
    bool EnableBatching = true;

    // 零拷贝配置
    bool EnableZeroCopy = true;
    size_t ZeroCopyThreshold = 1024;  // 零拷贝阈值

    // 预分配配置
    size_t PreallocatedMessages = 1000;  // 预分配消息数量
    bool EnablePreallocation = true;

    // 性能监控配置
    bool EnablePerformanceMonitoring = true;
    uint32_t MonitoringIntervalMs = 1000;  // 监控间隔
};

// 性能优化器接口
class IPerformanceOptimizer
{
public:
    virtual ~IPerformanceOptimizer() = default;

    // 初始化和生命周期
    virtual bool Initialize(const PerformanceConfig& Config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // 内存池操作
    virtual void* AllocateFromPool(size_t Size) = 0;
    virtual void DeallocateToPool(void* Ptr, size_t Size) = 0;
    virtual bool IsPoolAllocation(void* Ptr) const = 0;
    virtual void CompactPool() = 0;

    // 消息对象池操作
    virtual MessagePtr CreateMessage() = 0;
    virtual MessagePtr CreateMessage(MessageType Type) = 0;
    virtual MessagePtr CreateMessage(MessageType Type, const std::string& Payload) = 0;
    virtual void RecycleMessage(MessagePtr Message) = 0;

    // 零拷贝操作
    virtual ZeroCopyBuffer CreateZeroCopyBuffer(const void* Data, size_t Size) = 0;
    virtual ZeroCopyBuffer CreateZeroCopyBuffer(const std::string& Data) = 0;
    virtual void ReleaseZeroCopyBuffer(ZeroCopyBuffer& Buffer) = 0;
    virtual MessagePtr CreateMessageFromZeroCopy(const ZeroCopyBuffer& Buffer,
                                                 MessageType Type = MessageType::TEXT) = 0;

    // 批处理操作
    virtual uint32_t CreateBatch() = 0;
    virtual uint32_t CreateBatch(const std::string& QueueName) = 0;
    virtual bool AddToBatch(uint32_t BatchId, MessagePtr Message) = 0;
    virtual bool CommitBatch(uint32_t BatchId) = 0;
    virtual bool AbortBatch(uint32_t BatchId) = 0;
    virtual BatchMessage GetBatchInfo(uint32_t BatchId) const = 0;
    // 重置批处理：清空消息并重置时间；若已完成则重新开启该批次
    virtual bool ResetBatch(uint32_t BatchId, const std::string& QueueName = "") = 0;

    // 性能监控
    virtual PerformanceStats GetPerformanceStats() const = 0;
    virtual void ResetPerformanceStats() = 0;
    virtual void EnablePerformanceMonitoring(bool Enable) = 0;
    virtual bool IsPerformanceMonitoringEnabled() const = 0;

    // 配置管理
    virtual void UpdateConfig(const PerformanceConfig& Config) = 0;
    virtual PerformanceConfig GetConfig() const = 0;
};

// 性能优化器实现
class PerformanceOptimizer : public IPerformanceOptimizer
{
public:
    PerformanceOptimizer();
    virtual ~PerformanceOptimizer();

    // IPerformanceOptimizer 实现
    bool Initialize(const PerformanceConfig& Config) override;
    void Shutdown() override;
    bool IsInitialized() const override;

    // 内存池操作
    void* AllocateFromPool(size_t Size) override;
    void DeallocateToPool(void* Ptr, size_t Size) override;
    bool IsPoolAllocation(void* Ptr) const override;
    void CompactPool() override;

    // 消息对象池操作
    MessagePtr CreateMessage() override;
    MessagePtr CreateMessage(MessageType Type) override;
    MessagePtr CreateMessage(MessageType Type, const std::string& Payload) override;
    void RecycleMessage(MessagePtr Message) override;

    // 零拷贝操作
    ZeroCopyBuffer CreateZeroCopyBuffer(const void* Data, size_t Size) override;
    ZeroCopyBuffer CreateZeroCopyBuffer(const std::string& Data) override;
    void ReleaseZeroCopyBuffer(ZeroCopyBuffer& Buffer) override;
    MessagePtr CreateMessageFromZeroCopy(const ZeroCopyBuffer& Buffer,
                                         MessageType Type = MessageType::TEXT) override;

    // 批处理操作
    uint32_t CreateBatch() override;
    uint32_t CreateBatch(const std::string& QueueName) override;
    bool AddToBatch(uint32_t BatchId, MessagePtr Message) override;
    bool CommitBatch(uint32_t BatchId) override;
    bool AbortBatch(uint32_t BatchId) override;
    BatchMessage GetBatchInfo(uint32_t BatchId) const override;
    bool ResetBatch(uint32_t BatchId, const std::string& QueueName = "") override;

    // 性能监控
    PerformanceStats GetPerformanceStats() const override;
    void ResetPerformanceStats() override;
    void EnablePerformanceMonitoring(bool Enable) override;
    bool IsPerformanceMonitoringEnabled() const override;

    // 配置管理
    void UpdateConfig(const PerformanceConfig& Config) override;
    PerformanceConfig GetConfig() const override;

private:
    // 内部数据结构
    struct MessagePoolEntry
    {
        MessagePtr Message;
        uint64_t LastUsedTime = 0;
        uint32_t UseCount = 0;
    };

    // 成员变量
    std::atomic<bool> Initialized{false};
    std::atomic<bool> PerformanceMonitoringEnabled{true};
    PerformanceConfig Config;
    mutable std::shared_mutex ConfigMutex;

    // 内存池
    void* MemoryPoolData = nullptr;
    size_t MemoryPoolSize = 0;
    std::vector<MemoryBlock*> MemoryPoolBlocks;
    std::vector<MemoryBlock*> FreeBlocks;
    mutable std::mutex MemoryPoolMutex;

    // 消息对象池
    std::queue<MessagePtr> MessagePool;
    std::vector<MessagePoolEntry> MessagePoolEntries;
    mutable std::mutex MessagePoolMutex;

    // 批处理
    std::unordered_map<uint32_t, BatchMessage> ActiveBatches;
    std::atomic<uint32_t> NextBatchId{1};
    mutable std::mutex BatchesMutex;
    // 已完成（提交或中止）的批处理ID集合，用于幂等保护
    std::unordered_set<uint32_t> FinalizedBatches;

    // 性能统计
    mutable PerformanceStats Stats;
    mutable std::mutex StatsMutex;

    // 监控线程
    std::thread MonitoringThread;
    std::atomic<bool> StopMonitoring{false};

    // 内部方法
    void InitializeMemoryPool();
    void InitializeMessagePool();
    void ShutdownMemoryPool();
    void ShutdownMessagePool();

    MemoryBlock* FindFreeBlock(size_t Size);
    void MarkBlockAsUsed(MemoryBlock* Block);
    void MarkBlockAsFree(MemoryBlock* Block);

    void MonitoringThreadFunc();
    void UpdatePerformanceMetrics();

    template <typename T>
    void UpdateAverage(std::atomic<double>& Average,
                       std::atomic<uint64_t>& Total,
                       uint64_t NewValue,
                       uint64_t Count);
};

// 全局性能优化器实例
extern std::unique_ptr<PerformanceOptimizer> GlobalPerformanceOptimizer;

// 便捷函数
PerformanceOptimizer& GetPerformanceOptimizer();
bool InitializePerformanceOptimizer(const PerformanceConfig& Config = PerformanceConfig{});
void ShutdownPerformanceOptimizer();

}  // namespace Helianthus::MessageQueue
