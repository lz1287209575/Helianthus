#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace Helianthus::Network::Asio
{
// 缓冲区池配置
struct BufferPoolConfig
{
    size_t BufferSize = 4096;     // 单个缓冲区大小
    size_t InitialPoolSize = 32;  // 初始池大小
    size_t MaxPoolSize = 1024;    // 最大池大小
    size_t GrowStep = 8;          // 增长步长
    bool EnableZeroInit = false;  // 是否启用零初始化
};

// 智能缓冲区类
class PooledBuffer
{
public:
    PooledBuffer(char* Data, size_t Size, bool IsPooled = true);
    ~PooledBuffer();

    // 禁用拷贝，允许移动
    PooledBuffer(const PooledBuffer&) = delete;
    PooledBuffer& operator=(const PooledBuffer&) = delete;
    PooledBuffer(PooledBuffer&& Other) noexcept;
    PooledBuffer& operator=(PooledBuffer&& Other) noexcept;

    // 访问缓冲区数据
    char* Data() const
    {
        return DataPtr;
    }
    size_t Size() const
    {
        return BufferSize;
    }
    size_t Capacity() const
    {
        return BufferSize;
    }

    // 重置缓冲区（用于重用）
    void Reset();

    // 检查是否来自池
    bool IsPooled() const
    {
        return PooledFlag;
    }

private:
    char* DataPtr;
    size_t BufferSize;
    bool PooledFlag;
};

// 缓冲区池类
class BufferPool
{
public:
    explicit BufferPool(const BufferPoolConfig& Config = BufferPoolConfig{});
    ~BufferPool();

    // 获取缓冲区
    std::unique_ptr<PooledBuffer> Acquire();

    // 释放缓冲区（自动回收到池中）
    void Release(std::unique_ptr<PooledBuffer> Buffer);

    // 获取池统计信息
    struct PoolStats
    {
        size_t TotalBuffers = 0;
        size_t AvailableBuffers = 0;
        size_t InUseBuffers = 0;
        size_t BufferSize = 0;
        size_t TotalMemory = 0;
    };

    PoolStats GetStats() const;

private:
    void GrowPool();
    char* AllocateBuffer();
    void DeallocateBuffer(char* Buffer);

    BufferPoolConfig Config;
    mutable std::mutex PoolMutex;
    std::queue<char*> AvailableBuffers;
    std::vector<char*> AllocatedBuffers;
    size_t InUseCount = 0;
};

// 全局缓冲区池管理器
class BufferPoolManager
{
public:
    static BufferPoolManager& Instance();

    // 获取指定大小的缓冲区池
    BufferPool& GetPool(size_t BufferSize);

    // 获取默认缓冲区池
    BufferPool& GetDefaultPool();

    // 获取所有池的统计信息
    std::vector<BufferPool::PoolStats> GetAllPoolStats() const;

private:
    BufferPoolManager() = default;
    ~BufferPoolManager() = default;

    mutable std::mutex ManagerMutex;
    std::unordered_map<size_t, std::unique_ptr<BufferPool>> Pools;
    std::unique_ptr<BufferPool> DefaultPool;
};

// 便利函数
inline std::unique_ptr<PooledBuffer> AcquireBuffer(size_t Size = 4096)
{
    if (Size == 4096)
    {
        return BufferPoolManager::Instance().GetDefaultPool().Acquire();
    }
    else
    {
        return BufferPoolManager::Instance().GetPool(Size).Acquire();
    }
}

inline void ReleaseBuffer(std::unique_ptr<PooledBuffer> Buffer)
{
    if (Buffer && Buffer->IsPooled())
    {
        // 根据缓冲区大小找到对应的池
        auto& Manager = BufferPoolManager::Instance();
        auto& Pool = Manager.GetPool(Buffer->Size());
        Pool.Release(std::move(Buffer));
    }
}
}  // namespace Helianthus::Network::Asio
