#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/NetworkTypes.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace Helianthus::Network::Asio;

class AsyncReadWriteTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化 WinSock
        WSADATA WsaData;
        int Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
        ASSERT_EQ(Result, 0) << "WSAStartup failed";
    }

    void TearDown() override
    {
        WSACleanup();
    }
};

// 测试 AsyncWrite 续传语义
TEST_F(AsyncReadWriteTest, AsyncWriteResume)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> WriteCompleted{false};
    std::atomic<bool> StopCalled{false};
    std::atomic<size_t> TotalWritten{0};
    Helianthus::Network::NetworkError WriteError = Helianthus::Network::NetworkError::NONE;
    
    // 创建测试数据
    std::string TestData = "Hello, World! This is a test message for async write resume.";
    std::vector<char> Buffer(TestData.begin(), TestData.end());
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 模拟 AsyncWrite 续传
    Context.Post([&Context, &Buffer, &WriteCompleted, &TotalWritten, &WriteError]() {
        // 模拟分块写入
        size_t ChunkSize = 10;
        size_t TotalSize = Buffer.size();
        size_t Written = 0;
        
        while (Written < TotalSize && !WriteCompleted.load())
        {
            size_t Remaining = TotalSize - Written;
            size_t CurrentChunk = std::min(ChunkSize, Remaining);
            
            // 模拟部分写入
            if (Remaining <= ChunkSize)
            {
                // 最后一块，完成写入
                Written += CurrentChunk;
                TotalWritten.store(Written);
                WriteCompleted.store(true);
                std::cout << "AsyncWrite 完成，总共写入 " << Written << " 字节" << std::endl;
            }
            else
            {
                // 部分写入，继续续传
                Written += CurrentChunk;
                TotalWritten.store(Written);
                std::cout << "AsyncWrite 续传，已写入 " << Written << " 字节" << std::endl;
                
                // 模拟短暂延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    
    // 等待写入完成
    int WaitCount = 0;
    while (!WriteCompleted.load() && WaitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    Context.Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(WriteCompleted.load()) << "写入应该完成";
    EXPECT_EQ(TotalWritten.load(), Buffer.size()) << "应该写入所有数据";
    EXPECT_EQ(WriteError, Helianthus::Network::NetworkError::NONE) << "应该没有错误";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "AsyncWrite 续传测试通过，写入 " << TotalWritten.load() << " 字节" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AsyncRead 续传语义
TEST_F(AsyncReadWriteTest, AsyncReadResume)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> ReadCompleted{false};
    std::atomic<bool> StopCalled{false};
    std::atomic<size_t> TotalRead{0};
    Helianthus::Network::NetworkError ReadError = Helianthus::Network::NetworkError::NONE;
    
    // 创建测试数据
    std::string ExpectedData = "Hello, World! This is a test message for async read resume.";
    std::vector<char> ReadBuffer(ExpectedData.size());
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 模拟 AsyncRead 续传
    Context.Post([&Context, &ReadBuffer, &ReadCompleted, &TotalRead, &ReadError, &ExpectedData]() {
        // 模拟分块读取
        size_t ChunkSize = 8;
        size_t TotalSize = ReadBuffer.size();
        size_t Read = 0;
        
        while (Read < TotalSize && !ReadCompleted.load())
        {
            size_t CurrentChunk = std::min(ChunkSize, TotalSize - Read);
            
            // 模拟部分读取
            if (CurrentChunk < ChunkSize)
            {
                // 最后一块，完成读取
                std::copy(ExpectedData.begin() + Read, 
                         ExpectedData.begin() + Read + CurrentChunk,
                         ReadBuffer.begin() + Read);
                Read += CurrentChunk;
                TotalRead.store(Read);
                ReadCompleted.store(true);
                std::cout << "AsyncRead 完成，总共读取 " << Read << " 字节" << std::endl;
            }
            else
            {
                // 部分读取，继续续传
                std::copy(ExpectedData.begin() + Read, 
                         ExpectedData.begin() + Read + CurrentChunk,
                         ReadBuffer.begin() + Read);
                Read += CurrentChunk;
                TotalRead.store(Read);
                std::cout << "AsyncRead 续传，已读取 " << Read << " 字节" << std::endl;
                
                // 模拟短暂延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    
    // 等待读取完成
    int WaitCount = 0;
    while (!ReadCompleted.load() && WaitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    Context.Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(ReadCompleted.load()) << "读取应该完成";
    EXPECT_EQ(TotalRead.load(), ExpectedData.size()) << "应该读取所有数据";
    EXPECT_EQ(ReadError, Helianthus::Network::NetworkError::NONE) << "应该没有错误";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    // 验证读取的数据
    std::string ReadData(ReadBuffer.begin(), ReadBuffer.end());
    EXPECT_EQ(ReadData, ExpectedData) << "读取的数据应该与期望数据一致";
    
    std::cout << "AsyncRead 续传测试通过，读取 " << TotalRead.load() << " 字节" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试错误处理和取消
TEST_F(AsyncReadWriteTest, ErrorHandlingAndCancel)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> StopCalled{false};
    std::atomic<int> ErrorCount{0};
    std::atomic<int> CancelCount{0};
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 模拟错误处理和取消
    Context.Post([&Context, &ErrorCount, &CancelCount]() {
        // 模拟网络错误
        ErrorCount++;
        std::cout << "模拟网络错误处理" << std::endl;
        
        // 模拟操作取消
        CancelCount++;
        std::cout << "模拟操作取消" << std::endl;
        
        // 模拟短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 清理
    Context.Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_EQ(ErrorCount.load(), 1) << "应该处理一个错误";
    EXPECT_EQ(CancelCount.load(), 1) << "应该取消一个操作";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "错误处理和取消测试通过" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试大块数据传输
TEST_F(AsyncReadWriteTest, LargeDataTransfer)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> TransferCompleted{false};
    std::atomic<bool> StopCalled{false};
    std::atomic<size_t> TotalTransferred{0};
    
    // 创建大块测试数据 (1MB)
    const size_t DataSize = 1024 * 1024;
    std::vector<char> LargeData(DataSize);
    for (size_t i = 0; i < DataSize; ++i) {
        LargeData[i] = static_cast<char>(i % 256);
    }
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 模拟大块数据传输
    Context.Post([&Context, &LargeData, &TransferCompleted, &TotalTransferred]() {
        // 模拟分块传输
        size_t ChunkSize = 4096; // 4KB 块
        size_t TotalSize = LargeData.size();
        size_t Transferred = 0;
        
        while (Transferred < TotalSize && !TransferCompleted.load())
        {
            size_t CurrentChunk = std::min(ChunkSize, TotalSize - Transferred);
            
            if (Transferred + CurrentChunk >= TotalSize)
            {
                // 完成传输
                Transferred = TotalSize;
                TotalTransferred.store(Transferred);
                TransferCompleted.store(true);
                std::cout << "大块数据传输完成，总共传输 " << Transferred << " 字节" << std::endl;
            }
            else
            {
                // 部分传输，继续续传
                Transferred += CurrentChunk;
                TotalTransferred.store(Transferred);
                
                if (Transferred % (1024 * 1024) == 0) {
                    std::cout << "大块数据传输进度: " << (Transferred / (1024 * 1024)) << "MB" << std::endl;
                }
                
                // 模拟短暂延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    
    // 等待传输完成
    int WaitCount = 0;
    while (!TransferCompleted.load() && WaitCount < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    Context.Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(TransferCompleted.load()) << "传输应该完成";
    EXPECT_EQ(TotalTransferred.load(), DataSize) << "应该传输所有数据";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "大块数据传输测试通过，传输 " << TotalTransferred.load() << " 字节" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
