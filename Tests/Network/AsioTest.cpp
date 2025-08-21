#include "Shared/Network/Asio/AsyncUdpSocket.h"
#include "Shared/Network/Asio/IoContext.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#if defined(__linux__)
    #include <unistd.h>
    #include <string.h>
#endif

using namespace Helianthus::Network::Asio;

class AsioTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Context = std::make_unique<IoContext>();
    }

    void TearDown() override
    {
        if (Context)
        {
            Context->Stop();
        }
    }

    std::unique_ptr<IoContext> Context;
};

TEST_F(AsioTest, CrossThreadPost)
{
    std::atomic<bool> taskExecuted = false;
    std::atomic<int> taskCount = 0;

    // 启动事件循环线程
    std::thread eventLoopThread([this]() { Context->Run(); });

    // 等待一下确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 从另一个线程发送任务
    std::thread postThread(
        [this, &taskExecuted, &taskCount]()
        {
            for (int i = 0; i < 5; ++i)
            {
                Context->Post(
                    [&taskExecuted, &taskCount]()
                    {
                        taskExecuted = true;
                        taskCount++;
                    });
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

    postThread.join();

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Context->Stop();
    eventLoopThread.join();

    EXPECT_TRUE(taskExecuted);
    EXPECT_EQ(taskCount, 5);
}

TEST_F(AsioTest, DelayedTask)
{
    std::atomic<bool> delayedTaskExecuted = false;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point executionTime;

    // 启动事件循环线程
    std::thread eventLoopThread([this]() { Context->Run(); });

    // 等待一下确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    startTime = std::chrono::steady_clock::now();

    // 发送延迟任务
    Context->PostDelayed(
        [&delayedTaskExecuted, &executionTime]()
        {
            delayedTaskExecuted = true;
            executionTime = std::chrono::steady_clock::now();
        },
        50);  // 50ms 延迟

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Context->Stop();
    eventLoopThread.join();

    EXPECT_TRUE(delayedTaskExecuted);

    auto actualDelay =
        std::chrono::duration_cast<std::chrono::milliseconds>(executionTime - startTime).count();
    EXPECT_GE(actualDelay, 45);  // 允许一些误差
    EXPECT_LE(actualDelay, 100);
}

#if defined(__linux__)
TEST_F(AsioTest, EpollEdgeTriggered)
{
    // 这个测试验证 Epoll 的边沿触发模式
    // 通过创建一对管道来模拟 socket 事件

    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<bool> readEventTriggered = false;
    std::atomic<int> eventCount = 0;

    // 启动事件循环线程
    std::thread eventLoopThread([this]() { Context->Run(); });

    // 等待一下确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 将读端添加到 Reactor
    auto reactor = Context->GetReactor();
    ASSERT_TRUE(reactor);

    bool added = reactor->Add(
        static_cast<Fd>(pipefd[0]),
        EventMask::Read,
        [&readEventTriggered, &eventCount, pipefd](EventMask mask)
        {
            if ((static_cast<uint32_t>(mask) & static_cast<uint32_t>(EventMask::Read)) != 0)
            {
                readEventTriggered = true;
                eventCount++;

                // 读取数据
                char buffer[1024];
                read(pipefd[0], buffer, sizeof(buffer));
            }
        });

    ASSERT_TRUE(added);

    // 写入数据触发事件
    const char* data = "test data";
    write(pipefd[1], data, strlen(data));

    // 等待事件处理
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(readEventTriggered);
    EXPECT_EQ(eventCount, 1);

    // 再次写入数据，验证边沿触发（如果没有更多数据，不应该再次触发）
    write(pipefd[1], data, strlen(data));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 在边沿触发模式下，如果没有读取所有数据，不应该再次触发
    // 这里我们期望只触发一次，因为我们在回调中读取了数据
    EXPECT_EQ(eventCount, 2);

    // 清理
    reactor->Del(static_cast<Fd>(pipefd[0]));
    close(pipefd[0]);
    close(pipefd[1]);

    Context->Stop();
    eventLoopThread.join();
}
#else
TEST_F(AsioTest, EpollEdgeTriggered)
{
    GTEST_SKIP() << "Epoll 边沿触发用例依赖 pipe/read/write，仅在 Linux 下执行";
}
#endif
