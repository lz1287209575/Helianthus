#include "Shared/Network/Asio/ZeroCopyBuffer.h"

#include <chrono>
#include <random>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;

class ZeroCopyBufferTest : public ::testing::Test
{
protected:
    void SetUp() override {}
};

TEST_F(ZeroCopyBufferTest, BasicZeroCopyBuffer)
{
    // 测试基本的零拷贝缓冲区功能
    ZeroCopyBuffer Buffer;

    // 添加不同类型的片段
    std::string Str1 = "Hello";
    std::string Str2 = "World";
    std::vector<uint8_t> Bytes = {1, 2, 3, 4, 5};
    const char* CStr = "Test";

    Buffer.AddFragment(Str1);
    Buffer.AddFragment(Str2);
    Buffer.AddFragment(Bytes);
    Buffer.AddFragment(CStr);

    // 验证片段信息
    EXPECT_EQ(Buffer.GetFragmentCount(), 4);
    EXPECT_EQ(Buffer.GetTotalSize(), Str1.size() + Str2.size() + Bytes.size() + strlen(CStr));
    EXPECT_FALSE(Buffer.IsEmpty());

    // 验证片段内容
    const auto& Fragments = Buffer.GetFragments();
    EXPECT_EQ(Fragments[0].Size, Str1.size());
    EXPECT_EQ(Fragments[1].Size, Str2.size());
    EXPECT_EQ(Fragments[2].Size, Bytes.size());
    EXPECT_EQ(Fragments[3].Size, strlen(CStr));

    // 清空缓冲区
    Buffer.Clear();
    EXPECT_TRUE(Buffer.IsEmpty());
    EXPECT_EQ(Buffer.GetFragmentCount(), 0);
    EXPECT_EQ(Buffer.GetTotalSize(), 0);
}

TEST_F(ZeroCopyBufferTest, BufferFragmentCreation)
{
    // 测试 BufferFragment 的创建方法
    std::string Str = "Test String";
    std::vector<uint8_t> Bytes = {10, 20, 30, 40};
    const char* CStr = "C String";

    auto Fragment1 = BufferFragment::FromString(Str);
    auto Fragment2 = BufferFragment::FromBytes(Bytes);
    auto Fragment3 = BufferFragment::FromCString(CStr);

    EXPECT_EQ(Fragment1.Size, Str.size());
    EXPECT_EQ(Fragment1.Data, Str.data());

    EXPECT_EQ(Fragment2.Size, Bytes.size());
    EXPECT_EQ(Fragment2.Data, Bytes.data());

    EXPECT_EQ(Fragment3.Size, strlen(CStr));
    EXPECT_EQ(Fragment3.Data, CStr);
}

TEST_F(ZeroCopyBufferTest, ZeroCopyReadBuffer)
{
    // 测试零拷贝读取缓冲区
    ZeroCopyReadBuffer ReadBuffer;

    std::string Str1(10, 'A');
    std::string Str2(15, 'B');
    std::vector<uint8_t> Bytes(20, 0x42);

    ReadBuffer.AddTarget(Str1);
    ReadBuffer.AddTarget(Str2);
    ReadBuffer.AddTarget(Bytes);

    // 验证目标信息
    EXPECT_EQ(ReadBuffer.GetTargetCount(), 3);
    EXPECT_EQ(ReadBuffer.GetTotalTargetSize(), Str1.size() + Str2.size() + Bytes.size());
    EXPECT_FALSE(ReadBuffer.IsEmpty());

    // 验证目标内容
    const auto& Targets = ReadBuffer.GetTargets();
    EXPECT_EQ(Targets[0].second, Str1.size());
    EXPECT_EQ(Targets[1].second, Str2.size());
    EXPECT_EQ(Targets[2].second, Bytes.size());

    // 清空缓冲区
    ReadBuffer.Clear();
    EXPECT_TRUE(ReadBuffer.IsEmpty());
    EXPECT_EQ(ReadBuffer.GetTargetCount(), 0);
    EXPECT_EQ(ReadBuffer.GetTotalTargetSize(), 0);
}

TEST_F(ZeroCopyBufferTest, ZeroCopyResult)
{
    // 测试零拷贝操作结果
    ZeroCopyResult Result1(100, true);
    ZeroCopyResult Result2(0, false, EAGAIN);

    EXPECT_EQ(Result1.BytesTransferred, 100);
    EXPECT_TRUE(Result1.Success);
    EXPECT_EQ(Result1.ErrorCode, 0);

    EXPECT_EQ(Result2.BytesTransferred, 0);
    EXPECT_FALSE(Result2.Success);
    EXPECT_EQ(Result2.ErrorCode, EAGAIN);
}

TEST_F(ZeroCopyBufferTest, ZeroCopyIOSupport)
{
    // 测试零拷贝I/O支持检查
    EXPECT_TRUE(ZeroCopyIO::IsSupported());

    // 重置统计信息
    ZeroCopyIO::ResetStats();
    auto Stats = ZeroCopyIO::GetStats();

    EXPECT_EQ(Stats.TotalOperations, 0);
    EXPECT_EQ(Stats.TotalBytesTransferred, 0);
    EXPECT_EQ(Stats.AverageBytesPerOperation, 0.0);
    EXPECT_EQ(Stats.FailedOperations, 0);
}

TEST_F(ZeroCopyBufferTest, ConvenienceFunctions)
{
    // 测试便利函数
    auto Buffer = MakeZeroCopyBuffer();
    auto ReadBuffer = MakeZeroCopyReadBuffer();

    EXPECT_TRUE(Buffer.IsEmpty());
    EXPECT_TRUE(ReadBuffer.IsEmpty());

    Buffer.AddFragment("Test", 4);
    ReadBuffer.AddTarget(new char[10], 10);

    EXPECT_FALSE(Buffer.IsEmpty());
    EXPECT_FALSE(ReadBuffer.IsEmpty());
}

TEST_F(ZeroCopyBufferTest, EmptyBufferHandling)
{
    // 测试空缓冲区的处理
    ZeroCopyBuffer Buffer;
    ZeroCopyReadBuffer ReadBuffer;

    // 添加空片段应该被忽略
    Buffer.AddFragment(nullptr, 0);
    Buffer.AddFragment("", 0);
    Buffer.AddFragment(std::string{});
    Buffer.AddFragment(std::vector<uint8_t>{});

    EXPECT_TRUE(Buffer.IsEmpty());
    EXPECT_EQ(Buffer.GetFragmentCount(), 0);
    EXPECT_EQ(Buffer.GetTotalSize(), 0);

    // 添加空目标应该被忽略
    ReadBuffer.AddTarget(nullptr, 0);
    std::string EmptyStr;
    std::vector<uint8_t> EmptyBytes;
    ReadBuffer.AddTarget(EmptyStr);
    ReadBuffer.AddTarget(EmptyBytes);

    EXPECT_TRUE(ReadBuffer.IsEmpty());
    EXPECT_EQ(ReadBuffer.GetTargetCount(), 0);
    EXPECT_EQ(ReadBuffer.GetTotalTargetSize(), 0);
}

TEST_F(ZeroCopyBufferTest, LargeBufferHandling)
{
    // 测试大缓冲区的处理
    ZeroCopyBuffer Buffer;

    // 创建多个大片段
    std::vector<std::string> LargeStrings;
    for (int i = 0; i < 10; ++i)
    {
        LargeStrings.emplace_back(1000, 'A' + i);
        Buffer.AddFragment(LargeStrings.back());
    }

    EXPECT_EQ(Buffer.GetFragmentCount(), 10);
    EXPECT_EQ(Buffer.GetTotalSize(), 10 * 1000);

    // 验证所有片段都正确添加
    const auto& Fragments = Buffer.GetFragments();
    for (size_t i = 0; i < Fragments.size(); ++i)
    {
        EXPECT_EQ(Fragments[i].Size, LargeStrings[i].size());
        EXPECT_EQ(Fragments[i].Data, LargeStrings[i].data());
    }
}

TEST_F(ZeroCopyBufferTest, PerformanceStats)
{
    // 测试性能统计功能
    ZeroCopyIO::ResetStats();

    // 模拟一些操作
    ZeroCopyBuffer Buffer;
    Buffer.AddFragment("Test Data", 9);

    // 注意：这里我们不能实际调用 sendmsg/recvmsg，因为没有真实的socket
    // 但我们可以测试统计功能的基本结构

    auto Stats = ZeroCopyIO::GetStats();
    EXPECT_EQ(Stats.TotalOperations, 0);
    EXPECT_EQ(Stats.TotalBytesTransferred, 0);
    EXPECT_EQ(Stats.AverageBytesPerOperation, 0.0);
    EXPECT_EQ(Stats.FailedOperations, 0);

    // 重置统计信息
    ZeroCopyIO::ResetStats();
    Stats = ZeroCopyIO::GetStats();
    EXPECT_EQ(Stats.TotalOperations, 0);
    EXPECT_EQ(Stats.TotalBytesTransferred, 0);
    EXPECT_EQ(Stats.AverageBytesPerOperation, 0.0);
    EXPECT_EQ(Stats.FailedOperations, 0);
}

TEST_F(ZeroCopyBufferTest, MoveSemantics)
{
    // 测试移动语义
    ZeroCopyBuffer Buffer1;
    Buffer1.AddFragment("Hello", 5);
    Buffer1.AddFragment("World", 5);

    ZeroCopyBuffer Buffer2 = std::move(Buffer1);

    EXPECT_TRUE(Buffer1.IsEmpty());   // 原对象应该为空
    EXPECT_FALSE(Buffer2.IsEmpty());  // 新对象应该有数据
    EXPECT_EQ(Buffer2.GetFragmentCount(), 2);
    EXPECT_EQ(Buffer2.GetTotalSize(), 10);

    // 测试读取缓冲区的移动语义
    ZeroCopyReadBuffer ReadBuffer1;
    std::string Str = "Test";
    ReadBuffer1.AddTarget(Str);

    ZeroCopyReadBuffer ReadBuffer2 = std::move(ReadBuffer1);

    EXPECT_TRUE(ReadBuffer1.IsEmpty());
    EXPECT_FALSE(ReadBuffer2.IsEmpty());
    EXPECT_EQ(ReadBuffer2.GetTargetCount(), 1);
}
