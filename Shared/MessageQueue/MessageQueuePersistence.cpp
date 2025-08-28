#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/LogCategories.h"

#include <shared_mutex>

namespace Helianthus::MessageQueue
{

QueueResult MessageQueue::SaveToDisk()
{
    if (!Initialized.load() || !PersistenceMgr)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "开始保存消息队列数据到磁盘");
    {
        std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
        for (const auto& [QueueName, QueueData] : Queues)
        {
            auto Result = PersistenceMgr->SaveQueue(QueueName, QueueData->Config, QueueData->Stats);
            if (Result != QueueResult::SUCCESS)
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "保存队列失败 queue={} code={}", QueueName, static_cast<int>(Result));
                return Result;
            }
        }
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "消息队列数据保存到磁盘完成");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::LoadFromDisk()
{
    if (!Initialized.load() || !PersistenceMgr)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "开始从磁盘加载消息队列数据");
    auto PersistedQueues = PersistenceMgr->ListPersistedQueues();
    for (const auto& QueueName : PersistedQueues)
    {
        QueueConfig Config; QueueStats Stats;
        auto Result = PersistenceMgr->LoadQueue(QueueName, Config, Stats);
        if (Result != QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "加载队列失败 queue={} code={}", QueueName, static_cast<int>(Result));
            continue;
        }
        Result = CreateQueue(Config);
        if (Result != QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "创建队列失败 queue={} code={}", QueueName, static_cast<int>(Result));
            continue;
        }
        std::vector<MessagePtr> Messages;
        Result = PersistenceMgr->LoadAllMessages(QueueName, Messages);
        if (Result == QueueResult::SUCCESS)
        {
            auto Q = GetQueueData(QueueName);
            if (Q)
            {
                for (const auto& M : Messages) { Q->AddMessage(M); }
            }
        }
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "从磁盘加载消息队列数据完成");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::EnablePersistence(const std::string& QueueName, PersistenceMode Mode)
{
    // 占位：实际接入 PersistenceMgr 的模式切换
    (void)QueueName; (void)Mode;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DisablePersistence(const std::string& QueueName)
{
    // 占位：实际接入 PersistenceMgr 的模式切换
    (void)QueueName;
    return QueueResult::SUCCESS;
}

} // namespace Helianthus::MessageQueue


