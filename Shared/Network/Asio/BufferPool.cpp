#include "Shared/Network/Asio/BufferPool.h"
#include <cstring>
#include <algorithm>

namespace Helianthus::Network::Asio
{
    // PooledBuffer 实现
    PooledBuffer::PooledBuffer(char* Data, size_t Size, bool IsPooled)
        : DataPtr(Data)
        , BufferSize(Size)
        , PooledFlag(IsPooled)
    {
    }

    PooledBuffer::~PooledBuffer()
    {
        // 析构函数不释放内存，由池管理
    }

    PooledBuffer::PooledBuffer(PooledBuffer&& Other) noexcept
        : DataPtr(Other.DataPtr)
        , BufferSize(Other.BufferSize)
        , PooledFlag(Other.PooledFlag)
    {
        Other.DataPtr = nullptr;
        Other.BufferSize = 0;
        Other.PooledFlag = false;
    }

    PooledBuffer& PooledBuffer::operator=(PooledBuffer&& Other) noexcept
    {
        if (this != &Other) {
            DataPtr = Other.DataPtr;
            BufferSize = Other.BufferSize;
            PooledFlag = Other.PooledFlag;
            
            Other.DataPtr = nullptr;
            Other.BufferSize = 0;
            Other.PooledFlag = false;
        }
        return *this;
    }

    void PooledBuffer::Reset()
    {
        // 重置缓冲区内容（如果需要零初始化）
        if (DataPtr && PooledFlag) {
            std::memset(DataPtr, 0, BufferSize);
        }
    }

    // BufferPool 实现
    BufferPool::BufferPool(const BufferPoolConfig& ConfigIn)
        : Config(ConfigIn)
        , InUseCount(0)
    {
        // 预分配初始缓冲区
        for (size_t i = 0; i < Config.InitialPoolSize; ++i) {
            char* Buffer = AllocateBuffer();
            AvailableBuffers.push(Buffer);
        }
    }

    BufferPool::~BufferPool()
    {
        // 释放所有分配的缓冲区
        for (char* Buffer : AllocatedBuffers) {
            DeallocateBuffer(Buffer);
        }
    }

    std::unique_ptr<PooledBuffer> BufferPool::Acquire()
    {
        std::lock_guard<std::mutex> lock(PoolMutex);
        
        if (AvailableBuffers.empty()) {
            // 池为空，需要增长
            if (AllocatedBuffers.size() < Config.MaxPoolSize) {
                GrowPool();
            } else {
                // 达到最大池大小，分配非池化缓冲区
                char* Buffer = new char[Config.BufferSize];
                if (Config.EnableZeroInit) {
                    std::memset(Buffer, 0, Config.BufferSize);
                }
                return std::make_unique<PooledBuffer>(Buffer, Config.BufferSize, false);
            }
        }
        
        if (!AvailableBuffers.empty()) {
            char* Buffer = AvailableBuffers.front();
            AvailableBuffers.pop();
            InUseCount++;
            
            return std::make_unique<PooledBuffer>(Buffer, Config.BufferSize, true);
        }
        
        // 如果仍然没有可用缓冲区，分配非池化缓冲区
        char* Buffer = new char[Config.BufferSize];
        if (Config.EnableZeroInit) {
            std::memset(Buffer, 0, Config.BufferSize);
        }
        return std::make_unique<PooledBuffer>(Buffer, Config.BufferSize, false);
    }

    void BufferPool::Release(std::unique_ptr<PooledBuffer> Buffer)
    {
        if (!Buffer) return;
        
        std::lock_guard<std::mutex> lock(PoolMutex);
        
        if (Buffer->IsPooled()) {
            // 重置缓冲区内容
            if (Config.EnableZeroInit) {
                Buffer->Reset();
            }
            
            // 回收到池中
            AvailableBuffers.push(Buffer->Data());
            InUseCount--;
        } else {
            // 非池化缓冲区直接释放
            delete[] Buffer->Data();
        }
    }

    BufferPool::PoolStats BufferPool::GetStats() const
    {
        std::lock_guard<std::mutex> lock(PoolMutex);
        
        PoolStats Stats;
        Stats.TotalBuffers = AllocatedBuffers.size();
        Stats.AvailableBuffers = AvailableBuffers.size();
        Stats.InUseBuffers = InUseCount;
        Stats.BufferSize = Config.BufferSize;
        Stats.TotalMemory = Stats.TotalBuffers * Config.BufferSize;
        
        return Stats;
    }

    void BufferPool::GrowPool()
    {
        size_t GrowCount = std::min(Config.GrowStep, Config.MaxPoolSize - AllocatedBuffers.size());
        
        for (size_t i = 0; i < GrowCount; ++i) {
            char* Buffer = AllocateBuffer();
            AvailableBuffers.push(Buffer);
        }
    }

    char* BufferPool::AllocateBuffer()
    {
        char* Buffer = new char[Config.BufferSize];
        AllocatedBuffers.push_back(Buffer);
        
        if (Config.EnableZeroInit) {
            std::memset(Buffer, 0, Config.BufferSize);
        }
        
        return Buffer;
    }

    void BufferPool::DeallocateBuffer(char* Buffer)
    {
        delete[] Buffer;
    }

    // BufferPoolManager 实现
    BufferPoolManager& BufferPoolManager::Instance()
    {
        static BufferPoolManager Instance;
        return Instance;
    }

    BufferPool& BufferPoolManager::GetPool(size_t BufferSize)
    {
        std::lock_guard<std::mutex> lock(ManagerMutex);
        
        auto it = Pools.find(BufferSize);
        if (it == Pools.end()) {
            // 创建新的池
            BufferPoolConfig Config;
            Config.BufferSize = BufferSize;
            Pools[BufferSize] = std::make_unique<BufferPool>(Config);
        }
        
        return *Pools[BufferSize];
    }

    BufferPool& BufferPoolManager::GetDefaultPool()
    {
        std::lock_guard<std::mutex> lock(ManagerMutex);
        
        if (!DefaultPool) {
            DefaultPool = std::make_unique<BufferPool>();
        }
        
        return *DefaultPool;
    }

    std::vector<BufferPool::PoolStats> BufferPoolManager::GetAllPoolStats() const
    {
        std::lock_guard<std::mutex> lock(ManagerMutex);
        
        std::vector<BufferPool::PoolStats> Stats;
        
        if (DefaultPool) {
            Stats.push_back(DefaultPool->GetStats());
        }
        
        for (const auto& Pool : Pools) {
            Stats.push_back(Pool.second->GetStats());
        }
        
        return Stats;
    }
}
