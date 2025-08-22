#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

class CancelTimeoutTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Context = std::make_shared<IoContext>();
        ContextThread = std::thread([this]() { Context->Run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown() override
    {
        if (Context)
        {
            Context->Stop();
        }
        if (ContextThread.joinable())
        {
            ContextThread.join();
        }
    }

    std::shared_ptr<IoContext> Context;
    std::thread ContextThread;
};

TEST_F(CancelTimeoutTest, PostWithCancel)
{
    std::atomic<bool> TaskExecuted = false;
    std::atomic<bool> TaskCancelled = false;

    // 创建取消 token
    auto Token = Context->CreateCancelToken();

    // 提交任务
    auto TaskId = Context->PostWithCancel([&TaskExecuted]() {
        TaskExecuted = true;
    });
    (void)TaskId;

    // 立即取消任务
    Token->store(true);

    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 任务应该被取消，不执行
    EXPECT_FALSE(TaskExecuted.load());
}

TEST_F(CancelTimeoutTest, PostDelayedWithCancel)
{
    std::atomic<bool> TaskExecuted = false;

    // 创建取消 token
    auto Token = Context->CreateCancelToken();

    // 提交延迟任务（100ms 后执行）
    auto TaskId = Context->PostDelayedWithCancel([&TaskExecuted]() {
        TaskExecuted = true;
    }, 100, Token);
    (void)TaskId;

    // 等待 50ms 后取消任务
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    bool CancelResult = Context->CancelTask(TaskId);
    EXPECT_TRUE(CancelResult);

    // 再等待 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 任务应该被取消，不执行
    EXPECT_FALSE(TaskExecuted.load());
}

TEST_F(CancelTimeoutTest, CancelToken)
{
    std::atomic<bool> Task1Executed = false;
    std::atomic<bool> Task2Executed = false;

    // 创建共享的取消 token
    auto Token = Context->CreateCancelToken();

    // 提交两个任务
    auto TaskId1 = Context->PostWithCancel([&Task1Executed]() {
        Task1Executed = true;
    });
    (void)TaskId1;
    auto TaskId2 = Context->PostDelayedWithCancel([&Task2Executed]() {
        Task2Executed = true;
    }, 100, Token);
    (void)TaskId2;

    // 取消 token
    Token->store(true);

    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // 两个任务都应该被取消
    EXPECT_FALSE(Task1Executed.load());
    EXPECT_FALSE(Task2Executed.load());
}

TEST_F(CancelTimeoutTest, DelayedTaskExecution)
{
    std::atomic<bool> TaskExecuted = false;
    auto StartTime = std::chrono::steady_clock::now();

    // 提交延迟任务（50ms 后执行）
    auto TaskId = Context->PostDelayedWithCancel([&TaskExecuted]() {
        TaskExecuted = true;
    }, 50);
    (void)TaskId;

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    // 任务应该被执行
    EXPECT_TRUE(TaskExecuted.load());
    // 延迟应该在合理范围内
    EXPECT_GE(Duration.count(), 40); // 至少 40ms
    EXPECT_LE(Duration.count(), 100); // 最多 100ms
}

TEST_F(CancelTimeoutTest, MultipleDelayedTasks)
{
    std::vector<std::atomic<bool>> TaskExecuted(5);
    std::vector<IoContext::TaskId> TaskIds(5);

    // 提交多个延迟任务
    for (int I = 0; I < 5; ++I) {
        TaskIds[I] = Context->PostDelayedWithCancel([&TaskExecuted, I]() {
            TaskExecuted[I] = true;
        }, 50 + I * 10); // 50ms, 60ms, 70ms, 80ms, 90ms
    }

    // 取消中间的任务
    bool CancelResult = Context->CancelTask(TaskIds[2]);
    EXPECT_TRUE(CancelResult);

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // 检查结果
    EXPECT_TRUE(TaskExecuted[0].load());  // 应该执行
    EXPECT_TRUE(TaskExecuted[1].load());  // 应该执行
    EXPECT_FALSE(TaskExecuted[2].load()); // 被取消
    EXPECT_TRUE(TaskExecuted[3].load());  // 应该执行
    EXPECT_TRUE(TaskExecuted[4].load());  // 应该执行
}

TEST_F(CancelTimeoutTest, CancelNonExistentTask)
{
    // 尝试取消不存在的任务
    bool CancelResult = Context->CancelTask(999);
    EXPECT_FALSE(CancelResult);
}

TEST_F(CancelTimeoutTest, TokenReuse)
{
    std::atomic<bool> Task1Executed = false;
    std::atomic<bool> Task2Executed = false;

    // 创建取消 token
    auto Token = Context->CreateCancelToken();

    // 提交第一个任务
    auto TaskId1 = Context->PostWithCancel([&Task1Executed]() {
        Task1Executed = true;
    });
    (void)TaskId1;

    // 等待第一个任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(Task1Executed.load());

    // 重置 token
    Token->store(false);

    // 提交第二个任务
    auto TaskId2 = Context->PostDelayedWithCancel([&Task2Executed]() {
        Task2Executed = true;
    }, 50, Token);
    (void)TaskId2;

    // 等待第二个任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(Task2Executed.load());
}
