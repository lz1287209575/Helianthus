#include <gtest/gtest.h>
#include "Shared/Network/Asio/BufferPool.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>

using namespace Helianthus::Network::Asio;

class BufferPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
};

TEST_F(BufferPoolTest, BasicBufferPool)
{
    // 测试基本的缓冲区池功能
    BufferPoolConfig Config;
    Config.BufferSize = 1024;
    Config.InitialPoolSize = 4;
    Config.MaxPoolSize = 16;
    
    BufferPool Pool(Config);
    
    // 获取缓冲区
    auto Buffer1 = Pool.Acquire();
    auto Buffer2 = Pool.Acquire();
    
    EXPECT_NE(Buffer1, nullptr);
    EXPECT_NE(Buffer2, nullptr);
    EXPECT_EQ(Buffer1->Size(), 1024);
    EXPECT_EQ(Buffer2->Size(), 1024);
    EXPECT_TRUE(Buffer1->IsPooled());
    EXPECT_TRUE(Buffer2->IsPooled());
    
    // 写入数据
    const char* TestData = "Hello, Buffer Pool!";
    memcpy(Buffer1->Data(), TestData, strlen(TestData));
    
    // 验证数据
    EXPECT_EQ(std::string(Buffer1->Data()), TestData);
    
    // 释放缓冲区
    Pool.Release(std::move(Buffer1));
    Pool.Release(std::move(Buffer2));
    
    // 检查统计信息
    auto Stats = Pool.GetStats();
    EXPECT_EQ(Stats.TotalBuffers, 4);  // 初始池大小
    EXPECT_EQ(Stats.AvailableBuffers, 4);  // 所有缓冲区都可用
    EXPECT_EQ(Stats.InUseBuffers, 0);
    EXPECT_EQ(Stats.BufferSize, 1024);
    EXPECT_EQ(Stats.TotalMemory, 4 * 1024);
}

TEST_F(BufferPoolTest, PoolGrowth)
{
    // 测试池的自动增长
    BufferPoolConfig Config;
    Config.BufferSize = 512;
    Config.InitialPoolSize = 2;
    Config.MaxPoolSize = 8;
    Config.GrowStep = 2;
    
    BufferPool Pool(Config);
    
    std::vector<std::unique_ptr<PooledBuffer>> Buffers;
    
    // 获取超过初始池大小的缓冲区
    for (int i = 0; i < 6; ++i) {
        auto Buffer = Pool.Acquire();
        EXPECT_NE(Buffer, nullptr);
        EXPECT_EQ(Buffer->Size(), 512);
        Buffers.push_back(std::move(Buffer));
    }
    
    // 检查统计信息
    auto Stats = Pool.GetStats();
    EXPECT_GE(Stats.TotalBuffers, 6);  // 至少应该有6个缓冲区
    EXPECT_EQ(Stats.AvailableBuffers, 0);
    EXPECT_EQ(Stats.InUseBuffers, 6);
    
    // 释放所有缓冲区
    for (auto& Buffer : Buffers) {
        Pool.Release(std::move(Buffer));
    }
    
    // 检查释放后的统计信息
    Stats = Pool.GetStats();
    EXPECT_GE(Stats.TotalBuffers, 6);
    EXPECT_EQ(Stats.AvailableBuffers, 6);  // 6个缓冲区可用
    EXPECT_EQ(Stats.InUseBuffers, 0);
}

TEST_F(BufferPoolTest, NonPooledBuffer)
{
    // 测试非池化缓冲区（当池满时）
    BufferPoolConfig Config;
    Config.BufferSize = 256;
    Config.InitialPoolSize = 1;
    Config.MaxPoolSize = 1;  // 限制池大小为1
    
    BufferPool Pool(Config);
    
    auto Buffer1 = Pool.Acquire();
    auto Buffer2 = Pool.Acquire();  // 这应该是一个非池化缓冲区
    
    EXPECT_NE(Buffer1, nullptr);
    EXPECT_NE(Buffer2, nullptr);
    EXPECT_TRUE(Buffer1->IsPooled());
    EXPECT_FALSE(Buffer2->IsPooled());  // 第二个缓冲区应该是非池化的
    
    Pool.Release(std::move(Buffer1));
    Pool.Release(std::move(Buffer2));
}

TEST_F(BufferPoolTest, ZeroInitialization)
{
    // 测试零初始化功能
    BufferPoolConfig Config;
    Config.BufferSize = 128;
    Config.InitialPoolSize = 2;
    Config.EnableZeroInit = true;
    
    BufferPool Pool(Config);
    
    auto Buffer = Pool.Acquire();
    EXPECT_NE(Buffer, nullptr);
    
    // 检查缓冲区是否被零初始化
    for (size_t i = 0; i < Buffer->Size(); ++i) {
        EXPECT_EQ(Buffer->Data()[i], 0);
    }
    
    Pool.Release(std::move(Buffer));
}

TEST_F(BufferPoolTest, BufferPoolManager)
{
    // 测试全局缓冲区池管理器
    auto& Manager = BufferPoolManager::Instance();
    
    // 获取默认池
    auto& DefaultPool = Manager.GetDefaultPool();
    auto DefaultBuffer = DefaultPool.Acquire();
    EXPECT_NE(DefaultBuffer, nullptr);
    EXPECT_EQ(DefaultBuffer->Size(), 4096);  // 默认大小
    DefaultPool.Release(std::move(DefaultBuffer));
    
    // 获取自定义大小的池
    auto& CustomPool = Manager.GetPool(2048);
    auto CustomBuffer = CustomPool.Acquire();
    EXPECT_NE(CustomBuffer, nullptr);
    EXPECT_EQ(CustomBuffer->Size(), 2048);
    CustomPool.Release(std::move(CustomBuffer));
    
    // 检查所有池的统计信息
    auto AllStats = Manager.GetAllPoolStats();
    EXPECT_GE(AllStats.size(), 2);  // 至少应该有默认池和自定义池
}

TEST_F(BufferPoolTest, ConvenienceFunctions)
{
    // 测试便利函数
    auto Buffer1 = AcquireBuffer();  // 默认大小
    auto Buffer2 = AcquireBuffer(1024);  // 自定义大小
    
    EXPECT_NE(Buffer1, nullptr);
    EXPECT_NE(Buffer2, nullptr);
    EXPECT_EQ(Buffer1->Size(), 4096);
    EXPECT_EQ(Buffer2->Size(), 1024);
    
    ReleaseBuffer(std::move(Buffer1));
    ReleaseBuffer(std::move(Buffer2));
}

TEST_F(BufferPoolTest, ThreadSafety)
{
    // 测试多线程安全性
    BufferPoolConfig Config;
    Config.BufferSize = 1024;
    Config.InitialPoolSize = 10;
    Config.MaxPoolSize = 100;
    
    BufferPool Pool(Config);
    std::atomic<int> SuccessCount = 0;
    std::atomic<int> TotalOperations = 0;
    
    auto Worker = [&Pool, &SuccessCount, &TotalOperations]() {
        std::random_device Rd;
        std::mt19937 Gen(Rd());
        std::uniform_int_distribution<> Dis(1, 10);
        
        for (int i = 0; i < 100; ++i) {
            auto Buffer = Pool.Acquire();
            if (Buffer) {
                // 模拟一些工作
                std::this_thread::sleep_for(std::chrono::microseconds(Dis(Gen)));
                
                // 写入一些数据
                std::string Data = "Thread " + std::to_string(i);
                memcpy(Buffer->Data(), Data.c_str(), std::min(Data.length(), Buffer->Size()));
                
                Pool.Release(std::move(Buffer));
                SuccessCount++;
            }
            TotalOperations++;
        }
    };
    
    // 启动多个线程
    std::vector<std::thread> Threads;
    for (int i = 0; i < 4; ++i) {
        Threads.emplace_back(Worker);
    }
    
    // 等待所有线程完成
    for (auto& Thread : Threads) {
        Thread.join();
    }
    
    // 验证结果
    EXPECT_EQ(TotalOperations, 400);  // 4线程 * 100操作
    EXPECT_EQ(SuccessCount, 400);  // 所有操作都应该成功
    
    // 检查池状态
    auto Stats = Pool.GetStats();
    EXPECT_EQ(Stats.InUseBuffers, 0);  // 所有缓冲区都应该被释放
    EXPECT_GT(Stats.AvailableBuffers, 0);  // 应该有可用的缓冲区
}

TEST_F(BufferPoolTest, MemoryEfficiency)
{
    // 测试内存效率
    BufferPoolConfig Config;
    Config.BufferSize = 1024;
    Config.InitialPoolSize = 5;
    Config.MaxPoolSize = 20;
    
    BufferPool Pool(Config);
    
    // 记录初始统计信息
    auto InitialStats = Pool.GetStats();
    size_t InitialMemory = InitialStats.TotalMemory;
    
    // 进行多次分配和释放
    for (int Round = 0; Round < 10; ++Round) {
        std::vector<std::unique_ptr<PooledBuffer>> Buffers;
        
        // 分配缓冲区
        for (int i = 0; i < 8; ++i) {
            Buffers.push_back(Pool.Acquire());
        }
        
        // 释放缓冲区
        for (auto& Buffer : Buffers) {
            Pool.Release(std::move(Buffer));
        }
    }
    
    // 检查最终统计信息
    auto FinalStats = Pool.GetStats();
    size_t FinalMemory = FinalStats.TotalMemory;
    
    // 内存使用应该保持稳定（池化应该避免频繁的内存分配/释放）
    EXPECT_LE(FinalMemory, InitialMemory * 3);  // 内存使用不应该过度增长（允许一些增长）
    EXPECT_EQ(FinalStats.InUseBuffers, 0);  // 所有缓冲区都应该被释放
}
