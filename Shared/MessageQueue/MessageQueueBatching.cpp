#include "Shared/Common/LogCategories.h"
#include "Shared/Common/StructuredLogger.h"
#include "Shared/MessageQueue/MessageQueue.h"

#include <shared_mutex>

namespace Helianthus::MessageQueue
{

QueueResult
MessageQueue::CreateZeroCopyBuffer(const void* Data, size_t Size, ZeroCopyBuffer& OutBuffer)
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    OutBuffer.Data = const_cast<void*>(Data);
    OutBuffer.Size = Size;
    OutBuffer.Capacity = Size;
    OutBuffer.IsOwned = false;
    auto EndTime = std::chrono::high_resolution_clock::now();
    double ElapsedMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
    UpdatePerformanceStats("zero_copy", ElapsedMs, Size);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "创建零拷贝缓冲区: size={}", Size);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ReleaseZeroCopyBuffer(ZeroCopyBuffer& Buffer)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "释放零拷贝缓冲区");
    Buffer.Data = nullptr;
    Buffer.Size = 0;
    Buffer.Capacity = 0;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SendMessageZeroCopy(const std::string& QueueName,
                                              const ZeroCopyBuffer& Buffer)
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    // 为了确保接收端可直接从内部缓冲读取，这里复制为内部数据存储
    Msg->Payload.SetData(Buffer.Data, Buffer.Size);
    auto Result = SendMessage(QueueName, Msg);
    auto EndTime = std::chrono::high_resolution_clock::now();
    double ElapsedMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
    UpdatePerformanceStats("zero_copy", ElapsedMs, Buffer.Size);
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Verbose,
          "零拷贝发送消息: queue={}, size={}, code={}",
          QueueName,
          Buffer.Size,
          static_cast<int>(Result));
    return Result;
}

QueueResult MessageQueue::CreateBatch(uint32_t& OutBatchId)
{
    OutBatchId = NextBatchId++;
    BatchMessage Batch;
    Batch.BatchId = OutBatchId;
    Batch.CreateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    {
        std::shared_lock<std::shared_mutex> BufferCfgLock(BufferConfigMutex);
        Batch.ExpireTime = Batch.CreateTime + BufferConfigData.BatchTimeoutMs;
    }
    {
        std::lock_guard<std::mutex> BatchesLock(BatchesMutex);
        ActiveBatches[OutBatchId] = Batch;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建批处理: id={}", OutBatchId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CreateBatchForQueue(const std::string& QueueName, uint32_t& OutBatchId)
{
    auto Result = CreateBatch(OutBatchId);
    if (Result != QueueResult::SUCCESS)
    {
        return Result;
    }
    {
        std::lock_guard<std::mutex> BatchesLock(BatchesMutex);
        auto It = ActiveBatches.find(OutBatchId);
        if (It != ActiveBatches.end())
        {
            It->second.QueueName = QueueName;
        }
    }
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::AddToBatch(uint32_t BatchId, MessagePtr Message)
{
    auto T0 = std::chrono::high_resolution_clock::now();
    // 仅负责将消息加入批处理，不做自动提交；提交由调用方显式触发
    {
        std::lock_guard<std::mutex> BatchesLock(BatchesMutex);
        auto It = ActiveBatches.find(BatchId);
        if (It == ActiveBatches.end())
        {
            return QueueResult::INVALID_PARAMETER;
        }
        auto& Batch = It->second;
        Batch.Messages.push_back(std::move(Message));
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Verbose,
              "添加到批处理: batch_id={}, queue={}, count={}",
              BatchId,
              Batch.QueueName,
              Batch.Messages.size());

        // 不在此处自动提交，避免调用方在同一批次继续添加时出现批次被提前清空的问题
    }
    auto T1 = std::chrono::high_resolution_clock::now();
    double Ms = std::chrono::duration<double, std::milli>(T1 - T0).count();
    UpdatePerformanceStats("batch_add", Ms, 1);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CommitBatch(uint32_t BatchId)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    std::vector<MessagePtr> Messages;
    std::string QueueName;
    {
        std::lock_guard<std::mutex> BatchesLock(BatchesMutex);
        auto It = ActiveBatches.find(BatchId);
        if (It == ActiveBatches.end())
        {
            return QueueResult::INVALID_PARAMETER;
        }
        Messages = std::move(It->second.Messages);
        QueueName = It->second.QueueName;
        ActiveBatches.erase(It);
    }

    QueueResult Result = QueueResult::SUCCESS;
    if (!Messages.empty())
    {
        if (!QueueName.empty())
        {
            Result = SendBatchMessages(QueueName, Messages);
        }
        else
        {
            for (auto& Msg : Messages)
            {
                auto SendRes = SendMessage("", Msg);
                if (SendRes != QueueResult::SUCCESS)
                {
                    Result = SendRes;
                }
            }
        }
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    double ElapsedMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
    UpdatePerformanceStats("batch", ElapsedMs, Messages.size());
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "提交批处理: id={}, queue={}, messages={}",
          BatchId,
          QueueName,
          Messages.size());
    if (!QueueName.empty())
    {
        std::lock_guard<std::mutex> Ck(BatchCountersMutex);
        BatchCommitCountByQueue[QueueName] += 1;
        BatchMessageCountByQueue[QueueName] += Messages.size();
    }
    return Result;
}

QueueResult MessageQueue::AbortBatch(uint32_t BatchId)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "中止批处理: id={}", BatchId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetBatchInfo(uint32_t BatchId, BatchMessage& OutBatch) const
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取批处理信息: id={}", BatchId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetBatchCounters(const std::string& QueueName,
                                           uint64_t& OutCommitCount,
                                           uint64_t& OutMessageCount) const
{
    std::lock_guard<std::mutex> Ck(BatchCountersMutex);
    OutCommitCount = 0;
    OutMessageCount = 0;
    auto ItC = BatchCommitCountByQueue.find(QueueName);
    if (ItC != BatchCommitCountByQueue.end())
        OutCommitCount = ItC->second;
    auto ItM = BatchMessageCountByQueue.find(QueueName);
    if (ItM != BatchMessageCountByQueue.end())
        OutMessageCount = ItM->second;
    return QueueResult::SUCCESS;
}

void MessageQueue::ProcessBatchTimeout()
{
    const auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::lock_guard<std::mutex> BatchesLock(BatchesMutex);
    std::vector<uint32_t> ExpiredIds;
    for (const auto& Kv : ActiveBatches)
    {
        const auto& Batch = Kv.second;
        if (Batch.ExpireTime > 0 && Batch.ExpireTime <= Now)
        {
            ExpiredIds.push_back(Kv.first);
        }
    }
    for (auto Id : ExpiredIds)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "批处理超时: id={}", Id);
        ActiveBatches.erase(Id);
    }
}

void MessageQueue::CompressBatch(BatchMessage& Batch)
{
    Batch.OriginalSize = 0;
    for (const auto& Msg : Batch.Messages)
    {
        Batch.OriginalSize += Msg ? Msg->Payload.Data.size() : 0;
    }
    Batch.CompressedSize = Batch.OriginalSize;
    Batch.IsCompressed = false;
}

void MessageQueue::DecompressBatch(BatchMessage& Batch)
{
    Batch.IsCompressed = false;
}

}  // namespace Helianthus::MessageQueue
