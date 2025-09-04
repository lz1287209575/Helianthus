#include "Shared/MessageQueue/MessageQueue.h"

#include <sstream>
#include <string>

#include <gtest/gtest.h>

using namespace Helianthus::MessageQueue;

TEST(PrometheusPerfNewMetricsTest, ExportsBatchAndZeroCopyDurations)
{
    MessageQueue MQ;
    ASSERT_EQ(MQ.Initialize(), QueueResult::SUCCESS);
    QueueConfig C;
    C.Name = "perf_q";
    C.Persistence = PersistenceMode::MEMORY_ONLY;
    ASSERT_EQ(MQ.CreateQueue(C), QueueResult::SUCCESS);

    // 触发一次零拷贝路径
    const char* Data = "hello";
    ZeroCopyBuffer ZB{};
    ASSERT_EQ(MQ.CreateZeroCopyBuffer(Data, 5, ZB), QueueResult::SUCCESS);
    ASSERT_EQ(MQ.SendMessageZeroCopy(C.Name, ZB), QueueResult::SUCCESS);
    ASSERT_EQ(MQ.ReleaseZeroCopyBuffer(ZB), QueueResult::SUCCESS);

    // 触发一次批处理提交
    uint32_t BatchId = 0;
    ASSERT_EQ(MQ.CreateBatchForQueue(C.Name, BatchId), QueueResult::SUCCESS);
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    std::string Payload = "world";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(MQ.AddToBatch(BatchId, Msg), QueueResult::SUCCESS);
    ASSERT_EQ(MQ.CommitBatch(BatchId), QueueResult::SUCCESS);

    // 构造 Prometheus 文本（与示例一致）
    PerformanceStats PS{};
    ASSERT_EQ(MQ.GetPerformanceStats(PS), QueueResult::SUCCESS);
    std::ostringstream Os;
    Os << "# HELP helianthus_zero_copy_duration_ms Average zero-copy duration in ms\n";
    Os << "# TYPE helianthus_zero_copy_duration_ms gauge\n";
    Os << "helianthus_zero_copy_duration_ms " << PS.AverageZeroCopyTimeMs << "\n";
    Os << "# HELP helianthus_batch_duration_ms Average batch duration in ms\n";
    Os << "# TYPE helianthus_batch_duration_ms gauge\n";
    Os << "helianthus_batch_duration_ms " << PS.AverageBatchTimeMs << "\n";
    const std::string Body = Os.str();

    // 断言 HELP/TYPE 与指标名存在
    EXPECT_NE(Body.find("# HELP helianthus_zero_copy_duration_ms"), std::string::npos);
    EXPECT_NE(Body.find("# TYPE helianthus_zero_copy_duration_ms gauge"), std::string::npos);
    EXPECT_NE(Body.find("helianthus_zero_copy_duration_ms"), std::string::npos);
    EXPECT_NE(Body.find("# HELP helianthus_batch_duration_ms"), std::string::npos);
    EXPECT_NE(Body.find("# TYPE helianthus_batch_duration_ms gauge"), std::string::npos);
    EXPECT_NE(Body.find("helianthus_batch_duration_ms"), std::string::npos);

    MQ.Shutdown();
}
