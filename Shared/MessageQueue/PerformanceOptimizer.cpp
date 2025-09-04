#include "PerformanceOptimizer.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "Common/LogCategories.h"
#include "Common/StructuredLogger.h"

namespace Helianthus::MessageQueue
{

// 全局性能优化器实例
std::unique_ptr<PerformanceOptimizer> GlobalPerformanceOptimizer;

// 便捷函数实现
PerformanceOptimizer& GetPerformanceOptimizer()
{
    if (!GlobalPerformanceOptimizer)
    {
        GlobalPerformanceOptimizer = std::make_unique<PerformanceOptimizer>();
    }
    return *GlobalPerformanceOptimizer;
}

bool InitializePerformanceOptimizer(const PerformanceConfig& Config)
{
    return GetPerformanceOptimizer().Initialize(Config);
}

void ShutdownPerformanceOptimizer()
{
    if (GlobalPerformanceOptimizer)
    {
        GlobalPerformanceOptimizer->Shutdown();
    }
}

// PerformanceOptimizer 实现
PerformanceOptimizer::PerformanceOptimizer()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能优化器创建");
}

PerformanceOptimizer::~PerformanceOptimizer()
{
    Shutdown();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能优化器销毁");
}

bool PerformanceOptimizer::Initialize(const PerformanceConfig& InConfig)
{
    if (Initialized.load())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "性能优化器已经初始化");
        return true;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "初始化性能优化器");

    {
        std::unique_lock<std::shared_mutex> Lock(ConfigMutex);
        Config = InConfig;
    }

    // 初始化内存池
    if (Config.EnableMemoryPool)
    {
        InitializeMemoryPool();
    }

    // 初始化消息对象池
    if (Config.EnableMessagePool)
    {
        InitializeMessagePool();
    }

    // 启动监控线程
    if (Config.EnablePerformanceMonitoring)
    {
        StopMonitoring.store(false);
        MonitoringThread = std::thread(&PerformanceOptimizer::MonitoringThreadFunc, this);
    }

    Initialized.store(true);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能优化器初始化完成");
    return true;
}

void PerformanceOptimizer::Shutdown()
{
    if (!Initialized.load())
    {
        return;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭性能优化器");

    // 停止监控线程
    StopMonitoring.store(true);
    if (MonitoringThread.joinable())
    {
        MonitoringThread.join();
    }

    // 关闭内存池
    ShutdownMemoryPool();

    // 关闭消息对象池
    ShutdownMessagePool();

    Initialized.store(false);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能优化器关闭完成");
}

bool PerformanceOptimizer::IsInitialized() const
{
    return Initialized.load();
}

// 内存池操作
void* PerformanceOptimizer::AllocateFromPool(size_t Size)
{
    if (!Config.EnableMemoryPool || Size == 0)
    {
        return nullptr;
    }

    auto StartTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> Lock(MemoryPoolMutex);

    MemoryBlock* Block = FindFreeBlock(Size);
    void* Result = nullptr;

    if (Block)
    {
        MarkBlockAsUsed(Block);
        Result = Block->Data;

        Stats.MemoryPoolHits++;

        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Verbose,
              "从内存池分配: size={}, ptr={}",
              Size,
              Result);
    }
    else
    {
        // 回退到系统分配
        Result = std::malloc(Size);
        Stats.MemoryPoolMisses++;

        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Verbose,
              "系统分配: size={}, ptr={}",
              Size,
              Result);
    }

    Stats.TotalAllocations++;
    Stats.TotalBytesAllocated += Size;

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto DurationNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime).count();
    Stats.AverageAllocationTimeMs = static_cast<double>(DurationNs) / 1000000.0;

    return Result;
}

void PerformanceOptimizer::DeallocateToPool(void* Ptr, size_t Size)
{
    if (!Config.EnableMemoryPool || !Ptr)
    {
        std::free(Ptr);
        return;
    }

    std::lock_guard<std::mutex> Lock(MemoryPoolMutex);

    bool ReturnedToPool = false;
    for (auto* Block : MemoryPoolBlocks)
    {
        if (Block->Data == Ptr)
        {
            MarkBlockAsFree(Block);
            ReturnedToPool = true;
            break;
        }
    }

    if (!ReturnedToPool)
    {
        std::free(Ptr);
    }

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Verbose,
          "释放到{}: ptr={}, size={}",
          ReturnedToPool ? "内存池" : "系统",
          Ptr,
          Size);
}

bool PerformanceOptimizer::IsPoolAllocation(void* Ptr) const
{
    if (!Config.EnableMemoryPool || !Ptr)
    {
        return false;
    }

    std::lock_guard<std::mutex> Lock(MemoryPoolMutex);

    for (const auto* Block : MemoryPoolBlocks)
    {
        if (Block->Data == Ptr)
        {
            return true;
        }
    }

    return false;
}

void PerformanceOptimizer::CompactPool()
{
    if (!Config.EnableMemoryPool)
    {
        return;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "压缩内存池");

    std::lock_guard<std::mutex> Lock(MemoryPoolMutex);

    // 重新整理空闲块列表
    FreeBlocks.clear();
    for (auto* Block : MemoryPoolBlocks)
    {
        if (!Block->IsUsed)
        {
            FreeBlocks.push_back(Block);
        }
    }

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "内存池压缩完成，空闲块: {}",
          FreeBlocks.size());
}

// 消息对象池操作
MessagePtr PerformanceOptimizer::CreateMessage()
{
    return CreateMessage(MessageType::TEXT);
}

MessagePtr PerformanceOptimizer::CreateMessage(MessageType Type)
{
    if (!Config.EnableMessagePool)
    {
        auto Result = std::make_shared<Message>();
        Result->Header.Type = Type;
        return Result;
    }

    auto StartTime = std::chrono::high_resolution_clock::now();
    MessagePtr Result = nullptr;

    {
        std::lock_guard<std::mutex> Lock(MessagePoolMutex);

        if (!MessagePool.empty())
        {
            Result = MessagePool.front();
            MessagePool.pop();

            // 重置消息状态
            Result->Header.Id = 0;  // 使用简单的ID生成
            Result->Header.Type = Type;
            Result->Header.Priority = MessagePriority::NORMAL;
            Result->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
            Result->Header.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::system_clock::now().time_since_epoch())
                                           .count();
            Result->Header.ExpireTime = 0;
            Result->Header.MaxRetries = 3;
            Result->Header.RetryCount = 0;
            Result->Status = MessageStatus::PENDING;
            Result->Payload.Clear();
            Result->Header.Properties.clear();

            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Verbose,
                  "从对象池创建消息: type={}, id={}",
                  static_cast<int>(Type),
                  static_cast<uint64_t>(Result->Header.Id));
        }
    }

    if (!Result)
    {
        Result = std::make_shared<Message>();
        Result->Header.Type = Type;
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Verbose,
              "新建消息对象: type={}, id={}",
              static_cast<int>(Type),
              static_cast<uint64_t>(Result->Header.Id));
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto DurationNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime).count();

    return Result;
}

MessagePtr PerformanceOptimizer::CreateMessage(MessageType Type, const std::string& Payload)
{
    MessagePtr Message = CreateMessage(Type);
    // 使用Payload的SetExternal方法
    Message->Payload.SetExternal(const_cast<char*>(Payload.data()), Payload.size(), false, nullptr);
    return Message;
}

void PerformanceOptimizer::RecycleMessage(MessagePtr Message)
{
    if (!Config.EnableMessagePool || !Message)
    {
        return;
    }

    std::lock_guard<std::mutex> Lock(MessagePoolMutex);

    if (MessagePool.size() < Config.MessagePoolMaxSize)
    {
        MessagePool.push(Message);
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Verbose,
              "回收消息对象到池: id={}",
              static_cast<uint64_t>(Message->Header.Id));
    }
}

// 零拷贝操作
ZeroCopyBuffer PerformanceOptimizer::CreateZeroCopyBuffer(const void* Data, size_t Size)
{
    if (!Config.EnableZeroCopy || !Data || Size == 0)
    {
        return ZeroCopyBuffer();
    }

    auto StartTime = std::chrono::high_resolution_clock::now();

    ZeroCopyBuffer Buffer;
    Buffer.Data = const_cast<void*>(Data);
    Buffer.Size = Size;
    Buffer.Capacity = Size;
    Buffer.IsOwned = false;

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto DurationNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime).count();
    Stats.AverageZeroCopyTimeMs = static_cast<double>(DurationNs) / 1000000.0;
    Stats.ZeroCopyOperations++;

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "创建零拷贝缓冲区: size={}", Size);

    return Buffer;
}

ZeroCopyBuffer PerformanceOptimizer::CreateZeroCopyBuffer(const std::string& Data)
{
    return CreateZeroCopyBuffer(Data.data(), Data.size());
}

void PerformanceOptimizer::ReleaseZeroCopyBuffer(ZeroCopyBuffer& Buffer)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "释放零拷贝缓冲区: size={}", Buffer.Size);

    Buffer.Data = nullptr;
    Buffer.Size = 0;
    Buffer.Capacity = 0;
    Buffer.IsOwned = false;
}

MessagePtr PerformanceOptimizer::CreateMessageFromZeroCopy(const ZeroCopyBuffer& Buffer,
                                                           MessageType Type)
{
    if (!Buffer.Data || Buffer.Size == 0)
    {
        return nullptr;
    }

    MessagePtr Message = CreateMessage(Type);

    // 使用零拷贝设置负载
    Message->Payload.SetExternal(Buffer.Data, Buffer.Size, false, nullptr);

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Verbose,
          "从零拷贝缓冲区创建消息: type={}, size={}",
          static_cast<int>(Type),
          Buffer.Size);

    return Message;
}

// 批处理操作
uint32_t PerformanceOptimizer::CreateBatch()
{
    return CreateBatch("");
}

uint32_t PerformanceOptimizer::CreateBatch(const std::string& QueueName)
{
    uint32_t BatchId = NextBatchId.fetch_add(1, std::memory_order_relaxed);

    BatchMessage Batch;
    Batch.BatchId = BatchId;
    Batch.QueueName = QueueName;
    Batch.CreateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    Batch.ExpireTime = Batch.CreateTime + Config.BatchTimeoutMs;

    {
        std::lock_guard<std::mutex> Lock(BatchesMutex);
        ActiveBatches[BatchId] = Batch;
    }

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "创建批处理: id={}, queue={}",
          BatchId,
          QueueName);

    return BatchId;
}

bool PerformanceOptimizer::AddToBatch(uint32_t BatchId, MessagePtr Message)
{
    if (!Message)
    {
        return false;
    }

    bool shouldCommit = false;
    size_t currentCount = 0;
    {
        std::lock_guard<std::mutex> Lock(BatchesMutex);
        // 幂等：已完成的批不再接受添加
        if (FinalizedBatches.find(BatchId) != FinalizedBatches.end())
        {
            return false;
        }

        auto It = ActiveBatches.find(BatchId);
        if (It == ActiveBatches.end())
        {
            return false;
        }

        auto& Batch = It->second;
        Batch.Messages.push_back(Message);
        currentCount = Batch.Messages.size();

        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Verbose,
              "添加到批处理: batch_id={}, count={}",
              BatchId,
              currentCount);

        // 检查是否达到批处理大小（不在锁内提交，避免死锁）
        if (Config.EnableBatching && currentCount >= Config.BatchSize)
        {
            shouldCommit = true;
        }
    }

    if (shouldCommit)
    {
        return CommitBatch(BatchId);
    }

    return true;
}

bool PerformanceOptimizer::CommitBatch(uint32_t BatchId)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    std::vector<MessagePtr> Messages;
    std::string QueueName;

    {
        std::lock_guard<std::mutex> Lock(BatchesMutex);
        // 幂等：若已完成则直接返回成功
        if (FinalizedBatches.find(BatchId) != FinalizedBatches.end())
        {
            return true;
        }

        auto It = ActiveBatches.find(BatchId);
        if (It == ActiveBatches.end())
        {
            return false;
        }

        Messages = std::move(It->second.Messages);
        QueueName = It->second.QueueName;
        ActiveBatches.erase(It);
        FinalizedBatches.insert(BatchId);
    }

    // 这里应该将消息发送到队列
    // 由于我们没有直接访问MessageQueue实例，这里只是记录统计信息

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto DurationNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime).count();
    Stats.AverageBatchTimeMs = static_cast<double>(DurationNs) / 1000000.0;
    Stats.BatchOperations++;

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "提交批处理: id={}, messages={}",
          BatchId,
          Messages.size());

    return true;
}

bool PerformanceOptimizer::AbortBatch(uint32_t BatchId)
{
    std::lock_guard<std::mutex> Lock(BatchesMutex);
    // 幂等：若已完成则直接返回成功
    if (FinalizedBatches.find(BatchId) != FinalizedBatches.end())
    {
        return true;
    }

    auto It = ActiveBatches.find(BatchId);
    if (It == ActiveBatches.end())
    {
        return false;
    }

    ActiveBatches.erase(It);
    FinalizedBatches.insert(BatchId);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "中止批处理: id={}", BatchId);

    return true;
}

BatchMessage PerformanceOptimizer::GetBatchInfo(uint32_t BatchId) const
{
    std::lock_guard<std::mutex> Lock(BatchesMutex);

    auto It = ActiveBatches.find(BatchId);
    if (It == ActiveBatches.end())
    {
        return BatchMessage{};
    }

    return It->second;
}

bool PerformanceOptimizer::ResetBatch(uint32_t BatchId, const std::string& QueueName)
{
    std::lock_guard<std::mutex> Lock(BatchesMutex);

    // 如果该批次已完成，则将其从完成集合中移除并重新创建
    if (FinalizedBatches.find(BatchId) != FinalizedBatches.end())
    {
        FinalizedBatches.erase(BatchId);
    }

    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    BatchMessage& Batch = ActiveBatches[BatchId];
    Batch.BatchId = BatchId;
    if (!QueueName.empty())
    {
        Batch.QueueName = QueueName;
    }
    Batch.Messages.clear();
    Batch.CreateTime = Now;
    Batch.ExpireTime = Now + Config.BatchTimeoutMs;

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "重置批处理: id={}, queue={}",
          BatchId,
          Batch.QueueName);

    return true;
}

// 性能监控
PerformanceStats PerformanceOptimizer::GetPerformanceStats() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    return Stats;
}

void PerformanceOptimizer::ResetPerformanceStats()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    // 重置所有统计
    Stats.TotalAllocations = 0;
    Stats.TotalDeallocations = 0;
    Stats.TotalBytesAllocated = 0;
    Stats.CurrentBytesAllocated = 0;
    Stats.PeakBytesAllocated = 0;
    Stats.MemoryPoolHits = 0;
    Stats.MemoryPoolMisses = 0;
    Stats.MemoryPoolHitRate = 0.0;
    Stats.ZeroCopyOperations = 0;
    Stats.BatchOperations = 0;
    Stats.AverageAllocationTimeMs = 0.0;
    Stats.AverageDeallocationTimeMs = 0.0;
    Stats.AverageZeroCopyTimeMs = 0.0;
    Stats.AverageBatchTimeMs = 0.0;
    Stats.LastUpdateTime = 0;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能统计已重置");
}

void PerformanceOptimizer::EnablePerformanceMonitoring(bool Enable)
{
    PerformanceMonitoringEnabled.store(Enable);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能监控{}", Enable ? "启用" : "禁用");
}

bool PerformanceOptimizer::IsPerformanceMonitoringEnabled() const
{
    return PerformanceMonitoringEnabled.load();
}

// 配置管理
void PerformanceOptimizer::UpdateConfig(const PerformanceConfig& InConfig)
{
    std::unique_lock<std::shared_mutex> Lock(ConfigMutex);
    Config = InConfig;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能配置已更新");
}

PerformanceConfig PerformanceOptimizer::GetConfig() const
{
    std::shared_lock<std::shared_mutex> Lock(ConfigMutex);
    return Config;
}

// 私有方法实现
void PerformanceOptimizer::InitializeMemoryPool()
{
    std::lock_guard<std::mutex> Lock(MemoryPoolMutex);

    if (MemoryPoolData)
    {
        return;
    }

    MemoryPoolSize = Config.MemoryPoolSize;
    MemoryPoolData = std::malloc(MemoryPoolSize);

    if (!MemoryPoolData)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "内存池初始化失败: 分配 {} 字节失败",
              MemoryPoolSize);
        return;
    }

    // 切分为固定大小块
    const size_t BlockSize = std::max<size_t>(Config.BlockSize, 64);
    const size_t BlockCount = MemoryPoolSize / BlockSize;

    MemoryPoolBlocks.reserve(BlockCount);
    FreeBlocks.reserve(BlockCount);

    char* Base = static_cast<char*>(MemoryPoolData);
    for (size_t i = 0; i < BlockCount; ++i)
    {
        auto* Block = new MemoryBlock();
        Block->Data = Base + i * BlockSize;
        Block->Size = BlockSize;
        Block->IsUsed = false;
        Block->Next = nullptr;
        Block->AllocTime = 0;

        MemoryPoolBlocks.push_back(Block);
        FreeBlocks.push_back(Block);
    }

    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "内存池初始化完成: size={}, blocks={}",
          MemoryPoolSize,
          BlockCount);
}

void PerformanceOptimizer::InitializeMessagePool()
{
    std::lock_guard<std::mutex> Lock(MessagePoolMutex);

    if (Config.EnablePreallocation)
    {
        for (size_t i = 0; i < Config.PreallocatedMessages; ++i)
        {
            MessagePtr Message = std::make_shared<Helianthus::MessageQueue::Message>();
            MessagePool.push(Message);
        }

        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "消息对象池预分配完成: count={}",
              Config.PreallocatedMessages);
    }
}

void PerformanceOptimizer::ShutdownMemoryPool()
{
    std::lock_guard<std::mutex> Lock(MemoryPoolMutex);

    if (MemoryPoolData)
    {
        // 释放所有内存块
        for (auto* Block : MemoryPoolBlocks)
        {
            delete Block;
        }

        MemoryPoolBlocks.clear();
        FreeBlocks.clear();

        std::free(MemoryPoolData);
        MemoryPoolData = nullptr;
        MemoryPoolSize = 0;

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "内存池已关闭");
    }
}

void PerformanceOptimizer::ShutdownMessagePool()
{
    std::lock_guard<std::mutex> Lock(MessagePoolMutex);

    while (!MessagePool.empty())
    {
        MessagePool.pop();
    }

    MessagePoolEntries.clear();

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息对象池已关闭");
}

MemoryBlock* PerformanceOptimizer::FindFreeBlock(size_t Size)
{
    for (auto* Block : FreeBlocks)
    {
        if (Block->Size >= Size)
        {
            return Block;
        }
    }
    return nullptr;
}

void PerformanceOptimizer::MarkBlockAsUsed(MemoryBlock* Block)
{
    if (!Block)
    {
        return;
    }

    Block->IsUsed = true;
    Block->AllocTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

    // 从空闲列表中移除
    auto It = std::find(FreeBlocks.begin(), FreeBlocks.end(), Block);
    if (It != FreeBlocks.end())
    {
        FreeBlocks.erase(It);
    }
}

void PerformanceOptimizer::MarkBlockAsFree(MemoryBlock* Block)
{
    if (!Block)
    {
        return;
    }

    Block->IsUsed = false;

    // 添加到空闲列表
    FreeBlocks.push_back(Block);
}

void PerformanceOptimizer::MonitoringThreadFunc()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能监控线程启动");

    while (!StopMonitoring.load())
    {
        UpdatePerformanceMetrics();

        // 休眠监控间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(Config.MonitoringIntervalMs));
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "性能监控线程停止");
}

void PerformanceOptimizer::UpdatePerformanceMetrics()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);

    // 计算内存池命中率
    if (Stats.TotalAllocations > 0)
    {
        Stats.MemoryPoolHitRate =
            static_cast<double>(Stats.MemoryPoolHits) / Stats.TotalAllocations * 100.0;
    }

    // 更新最后更新时间
    Stats.LastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

    // 记录性能指标
    if (PerformanceMonitoringEnabled.load())
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "性能指标 - 内存池命中率: {:.2f}%, 零拷贝操作: {}, 批处理操作: {}, "
              "平均分配时间: {:.2f} ms, 平均零拷贝时间: {:.2f} ms, 平均批处理时间: {:.2f} ms",
              Stats.MemoryPoolHitRate,
              Stats.ZeroCopyOperations,
              Stats.BatchOperations,
              Stats.AverageAllocationTimeMs,
              Stats.AverageZeroCopyTimeMs,
              Stats.AverageBatchTimeMs);
    }
}

}  // namespace Helianthus::MessageQueue
