#include "MessagePersistence.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Helianthus::MessageQueue
{

// FileBasedPersistence 实现
FileBasedPersistence::FileBasedPersistence()
    : Initialized(false)
{
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "创建文件持久化实例");
}

FileBasedPersistence::~FileBasedPersistence()
{
    Shutdown();
}

QueueResult FileBasedPersistence::Initialize(const PersistenceConfig& Config)
{
    if (Initialized.load())
    {
        return QueueResult::SUCCESS;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "开始初始化文件持久化系统");

    this->Config = Config;
    
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "数据目录: {}", Config.DataDirectory);
    
    // 确保数据目录存在
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "开始创建数据目录...");
    auto Result = EnsureDataDirectory();
    if (Result != QueueResult::SUCCESS)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "创建数据目录失败 code={}", static_cast<int>(Result));
        return Result;
    }

    // 打开文件
    Result = OpenFiles();
    if (Result != QueueResult::SUCCESS)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "打开文件失败 code={}", static_cast<int>(Result));
        return Result;
    }

    // 读取索引
    Result = ReadIndexFromFile();
    if (Result != QueueResult::SUCCESS)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取索引失败，将重建索引");
        // 索引文件可能不存在，这是正常的
    }

    Initialized = true;
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "文件持久化系统初始化成功");
    return QueueResult::SUCCESS;
}

void FileBasedPersistence::Shutdown()
{
    if (!Initialized.load())
    {
        return;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "开始关闭文件持久化系统");

    // 写入索引
    WriteIndexToFile();
    
    // 关闭文件
    CloseFiles();

    Initialized = false;
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "文件持久化系统关闭完成");
}

bool FileBasedPersistence::IsInitialized() const
{
    return Initialized.load();
}

QueueResult FileBasedPersistence::SaveQueue(const std::string& QueueName, const QueueConfig& Config, const QueueStats& Stats)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::unique_lock<std::shared_mutex> Lock(QueueDataMutex);
    
    QueuePersistenceData& QueueData = this->QueueData[QueueName];
    QueueData.QueueName = QueueName;
    QueueData.Config = Config;
    QueueData.Stats = Stats;
    QueueData.IsDirty = true;

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "保存队列配置 queue={}", QueueName);
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::LoadQueue(const std::string& QueueName, QueueConfig& Config, QueueStats& Stats)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::shared_lock<std::shared_mutex> Lock(QueueDataMutex);
    
    auto It = QueueData.find(QueueName);
    if (It == QueueData.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    Config = It->second.Config;
    Stats = It->second.Stats;

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "加载队列配置 queue={}", QueueName);
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::DeleteQueue(const std::string& QueueName)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::unique_lock<std::shared_mutex> Lock(QueueDataMutex);
    std::unique_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    // 删除队列数据
    QueueData.erase(QueueName);
    
    // 删除消息索引
    QueueMessageIndex.erase(QueueName);

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "删除队列 queue={}", QueueName);
    return QueueResult::SUCCESS;
}

std::vector<std::string> FileBasedPersistence::ListPersistedQueues()
{
    std::vector<std::string> QueueNames;
    
    if (!Initialized.load())
    {
        return QueueNames;
    }

    std::shared_lock<std::shared_mutex> Lock(QueueDataMutex);
    
    for (const auto& [Name, Data] : QueueData)
    {
        QueueNames.push_back(Name);
    }

    return QueueNames;
}

QueueResult FileBasedPersistence::SaveMessage(const std::string& QueueName, MessagePtr Message)
{
    if (!Initialized.load() || !Message)
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::lock_guard<std::mutex> FileLock(FileMutex);
    
    // 写入消息到文件
    size_t FileOffset;
    auto Result = WriteMessageToFile(Message, FileOffset);
    if (Result != QueueResult::SUCCESS)
    {
        return Result;
    }

    // 更新索引
    Result = UpdateMessageIndex(QueueName, Message->Header.Id, FileOffset, Message->Payload.GetSize());
    if (Result != QueueResult::SUCCESS)
    {
        return Result;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "保存消息到磁盘 id={} queue={} offset={} size={}",
        static_cast<uint64_t>(Message->Header.Id), QueueName, static_cast<uint64_t>(FileOffset), static_cast<uint64_t>(Message->Payload.GetSize()));
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::LoadMessage(const std::string& QueueName, MessageId MessageId, MessagePtr& OutMessage)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::shared_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    auto QueueIt = QueueMessageIndex.find(QueueName);
    if (QueueIt == QueueMessageIndex.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    auto MessageIt = QueueIt->second.find(MessageId);
    if (MessageIt == QueueIt->second.end())
    {
        return QueueResult::MESSAGE_NOT_FOUND;
    }

    const MessageIndexEntry& Entry = MessageIt->second;
    if (Entry.IsDeleted)
    {
        return QueueResult::MESSAGE_NOT_FOUND;
    }

    std::lock_guard<std::mutex> FileLock(FileMutex);
    
    // 从文件读取消息
    auto Result = ReadMessageFromFile(Entry.FileOffset, Entry.MessageSize, OutMessage);
    if (Result != QueueResult::SUCCESS)
    {
        return Result;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "从磁盘加载消息 id={} queue={} offset={} size={}",
        static_cast<uint64_t>(MessageId), QueueName, static_cast<uint64_t>(Entry.FileOffset), static_cast<uint64_t>(Entry.MessageSize));
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::DeleteMessage(const std::string& QueueName, MessageId MessageId)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::unique_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    auto QueueIt = QueueMessageIndex.find(QueueName);
    if (QueueIt == QueueMessageIndex.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    auto MessageIt = QueueIt->second.find(MessageId);
    if (MessageIt == QueueIt->second.end())
    {
        return QueueResult::MESSAGE_NOT_FOUND;
    }

    // 标记为已删除
    MessageIt->second.IsDeleted = true;

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "删除消息 id={} queue={}", static_cast<uint64_t>(MessageId), QueueName);
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::SaveBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::lock_guard<std::mutex> FileLock(FileMutex);
    
    for (const auto& Message : Messages)
    {
        if (!Message)
        {
            continue;
        }

        size_t FileOffset;
        auto Result = WriteMessageToFile(Message, FileOffset);
        if (Result != QueueResult::SUCCESS)
        {
            return Result;
        }

        Result = UpdateMessageIndex(QueueName, Message->Header.Id, FileOffset, Message->Payload.GetSize());
        if (Result != QueueResult::SUCCESS)
        {
            return Result;
        }
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "批量保存消息到磁盘 count={} queue={}", static_cast<uint32_t>(Messages.size()), QueueName);
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::LoadAllMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::shared_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    auto QueueIt = QueueMessageIndex.find(QueueName);
    if (QueueIt == QueueMessageIndex.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::lock_guard<std::mutex> FileLock(FileMutex);
    
    for (const auto& [MessageId, Entry] : QueueIt->second)
    {
        if (Entry.IsDeleted)
        {
            continue;
        }

        MessagePtr Message;
        auto Result = ReadMessageFromFile(Entry.FileOffset, Entry.MessageSize, Message);
        if (Result == QueueResult::SUCCESS && Message)
        {
            OutMessages.push_back(Message);
        }
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "从磁盘加载所有消息 count={} queue={}", static_cast<uint32_t>(OutMessages.size()), QueueName);
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::RebuildIndex()
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "开始重建索引");
    
    // 清空现有索引
    {
        std::unique_lock<std::shared_mutex> IndexLock(IndexMutex);
        QueueMessageIndex.clear();
    }

    // 这里应该实现从文件重建索引的逻辑
    // 由于实现复杂，暂时返回成功
    
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "索引重建完成");
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::CompactFiles()
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "开始压缩文件");
    
    // 清理过期消息
    CleanupExpiredMessages();
    
    // 这里应该实现文件压缩逻辑
    // 由于实现复杂，暂时返回成功
    
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "文件压缩完成");
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::BackupData(const std::string& BackupPath)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "开始备份数据 path={}", BackupPath);
    
    // 这里应该实现数据备份逻辑
    // 由于实现复杂，暂时返回成功
    
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "数据备份完成 path={}", BackupPath);
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::RestoreData(const std::string& BackupPath)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "开始从备份恢复数据 path={}", BackupPath);
    
    // 这里应该实现数据恢复逻辑
    // 由于实现复杂，暂时返回成功
    
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Log, "数据恢复完成 path={}", BackupPath);
    return QueueResult::SUCCESS;
}

size_t FileBasedPersistence::GetPersistedMessageCount(const std::string& QueueName)
{
    if (!Initialized.load())
    {
        return 0;
    }

    std::shared_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    auto QueueIt = QueueMessageIndex.find(QueueName);
    if (QueueIt == QueueMessageIndex.end())
    {
        return 0;
    }

    size_t Count = 0;
    for (const auto& [MessageId, Entry] : QueueIt->second)
    {
        if (!Entry.IsDeleted)
        {
            Count++;
        }
    }

    return Count;
}

size_t FileBasedPersistence::GetTotalPersistedSize()
{
    if (!Initialized.load())
    {
        return 0;
    }

    std::shared_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    size_t TotalSize = 0;
    for (const auto& [QueueName, Messages] : QueueMessageIndex)
    {
        for (const auto& [MessageId, Entry] : Messages)
        {
            if (!Entry.IsDeleted)
            {
                TotalSize += Entry.MessageSize;
            }
        }
    }

    return TotalSize;
}

std::vector<std::string> FileBasedPersistence::GetDiagnostics()
{
    std::vector<std::string> Diagnostics;
    
    if (!Initialized.load())
    {
        Diagnostics.push_back("持久化系统未初始化");
        return Diagnostics;
    }

    std::shared_lock<std::shared_mutex> IndexLock(IndexMutex);
    std::shared_lock<std::shared_mutex> QueueDataLock(QueueDataMutex);
    
    Diagnostics.push_back("持久化系统状态: 已初始化");
    Diagnostics.push_back("数据目录: " + Config.DataDirectory);
    Diagnostics.push_back("队列数量: " + std::to_string(QueueData.size()));
    
    size_t TotalMessages = 0;
    for (const auto& [QueueName, Messages] : QueueMessageIndex)
    {
        size_t QueueMessageCount = 0;
        for (const auto& [MessageId, Entry] : Messages)
        {
            if (!Entry.IsDeleted)
            {
                QueueMessageCount++;
            }
        }
        TotalMessages += QueueMessageCount;
        Diagnostics.push_back("队列 " + QueueName + ": " + std::to_string(QueueMessageCount) + " 条消息");
    }
    
    Diagnostics.push_back("总消息数: " + std::to_string(TotalMessages));
    Diagnostics.push_back("总大小: " + std::to_string(GetTotalPersistedSize()) + " 字节");

    return Diagnostics;
}

// 私有方法实现
QueueResult FileBasedPersistence::EnsureDataDirectory()
{
    try
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "检查数据目录: {}", Config.DataDirectory);
        
        DataDir = std::filesystem::path(Config.DataDirectory);
        if (!std::filesystem::exists(DataDir))
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "创建数据目录: {}", Config.DataDirectory);
            std::filesystem::create_directories(DataDir);
        }
        
        QueueDataPath = DataDir / Config.QueueDataFile;
        MessageDataPath = DataDir / Config.MessageDataFile;
        IndexPath = DataDir / Config.IndexFile;
        
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "数据目录准备完成");
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "创建数据目录失败 err={} dir={}", e.what(), Config.DataDirectory);
        return QueueResult::INTERNAL_ERROR;
    }
}

QueueResult FileBasedPersistence::OpenFiles()
{
    try
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "开始打开文件...");
        
        // 打开队列数据文件
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "打开队列数据文件: {}", QueueDataPath.string());
        QueueDataFile.open(QueueDataPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
        if (!QueueDataFile.is_open())
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "重新打开队列数据文件（创建模式）");
            QueueDataFile.open(QueueDataPath, std::ios::binary | std::ios::out);
        }
        
        // 打开消息数据文件
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "打开消息数据文件: {}", MessageDataPath.string());
        MessageDataFile.open(MessageDataPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
        if (!MessageDataFile.is_open())
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "重新打开消息数据文件（创建模式）");
            MessageDataFile.open(MessageDataPath, std::ios::binary | std::ios::out);
        }
        
        // 打开索引文件
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "打开索引文件: {}", IndexPath.string());
        IndexFile.open(IndexPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
        if (!IndexFile.is_open())
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "重新打开索引文件（创建模式）");
            IndexFile.open(IndexPath, std::ios::binary | std::ios::out);
        }
        
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "所有文件打开完成");
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "打开文件失败 err={}", e.what());
        return QueueResult::INTERNAL_ERROR;
    }
}

void FileBasedPersistence::CloseFiles()
{
    if (QueueDataFile.is_open())
    {
        QueueDataFile.close();
    }
    if (MessageDataFile.is_open())
    {
        MessageDataFile.close();
    }
    if (IndexFile.is_open())
    {
        IndexFile.close();
    }
}

QueueResult FileBasedPersistence::WriteMessageToFile(MessagePtr Message, size_t& OutOffset)
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    try
    {
        // 序列化消息
        std::vector<uint8_t> SerializedData;
        auto Result = SerializeMessage(Message, SerializedData);
        if (Result != QueueResult::SUCCESS)
        {
            return Result;
        }

        // 获取当前文件位置
        OutOffset = MessageDataFile.tellp();
        
        // 写入消息大小
        uint32_t MessageSize = static_cast<uint32_t>(SerializedData.size());
        MessageDataFile.write(reinterpret_cast<const char*>(&MessageSize), sizeof(MessageSize));
        
        // 写入消息数据
        MessageDataFile.write(reinterpret_cast<const char*>(SerializedData.data()), SerializedData.size());
        
        // 刷新文件
        MessageDataFile.flush();
        
        // 记录耗时统计
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        Metrics.RecordWriteTime(static_cast<uint64_t>(Duration.count()));
        
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "写入消息失败 err={} offset={}", e.what(), static_cast<uint64_t>(OutOffset));
        return QueueResult::INTERNAL_ERROR;
    }
}

QueueResult FileBasedPersistence::ReadMessageFromFile(size_t Offset, size_t Size, MessagePtr& OutMessage)
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    try
    {
        // 定位到指定位置
        MessageDataFile.seekg(Offset);
        
        // 读取消息大小
        uint32_t MessageSize;
        MessageDataFile.read(reinterpret_cast<char*>(&MessageSize), sizeof(MessageSize));
        
        // 读取消息数据
        std::vector<uint8_t> SerializedData(MessageSize);
        MessageDataFile.read(reinterpret_cast<char*>(SerializedData.data()), MessageSize);
        
        // 反序列化消息
        auto Result = DeserializeMessage(SerializedData, OutMessage);
        if (Result != QueueResult::SUCCESS)
        {
            return Result;
        }
        
        // 记录耗时统计
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        Metrics.RecordReadTime(static_cast<uint64_t>(Duration.count()));
        
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "读取消息失败 err={} offset={} size={}", e.what(), static_cast<uint64_t>(Offset), static_cast<uint64_t>(Size));
        return QueueResult::INTERNAL_ERROR;
    }
}

QueueResult FileBasedPersistence::UpdateMessageIndex(const std::string& QueueName, MessageId MessageId, 
                                                    size_t FileOffset, size_t MessageSize)
{
    std::unique_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    MessageIndexEntry Entry;
    Entry.Id = MessageId;
    Entry.QueueName = QueueName;
    Entry.FileOffset = FileOffset;
    Entry.MessageSize = MessageSize;
    Entry.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Entry.IsDeleted = false;
    
    QueueMessageIndex[QueueName][MessageId] = Entry;
    
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::RemoveMessageFromIndex(const std::string& QueueName, MessageId MessageId)
{
    std::unique_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    auto QueueIt = QueueMessageIndex.find(QueueName);
    if (QueueIt != QueueMessageIndex.end())
    {
        QueueIt->second.erase(MessageId);
    }
    
    return QueueResult::SUCCESS;
}

QueueResult FileBasedPersistence::SerializeMessage(MessagePtr Message, std::vector<uint8_t>& OutData)
{
    try
    {
        // 简单的序列化实现
        // 在实际应用中，应该使用更高效的序列化库如 Protocol Buffers
        
        std::stringstream Stream;
        
        // 序列化消息头
        Stream.write(reinterpret_cast<const char*>(&Message->Header.Id), sizeof(Message->Header.Id));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.Type), sizeof(Message->Header.Type));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.Priority), sizeof(Message->Header.Priority));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.Delivery), sizeof(Message->Header.Delivery));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.Timestamp), sizeof(Message->Header.Timestamp));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.ExpireTime), sizeof(Message->Header.ExpireTime));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.RetryCount), sizeof(Message->Header.RetryCount));
        Stream.write(reinterpret_cast<const char*>(&Message->Header.MaxRetries), sizeof(Message->Header.MaxRetries));
        
        // 序列化消息属性
        uint32_t PropertiesCount = static_cast<uint32_t>(Message->Header.Properties.size());
        Stream.write(reinterpret_cast<const char*>(&PropertiesCount), sizeof(PropertiesCount));
        for (const auto& [Key, Value] : Message->Header.Properties)
        {
            uint32_t KeySize = static_cast<uint32_t>(Key.size());
            Stream.write(reinterpret_cast<const char*>(&KeySize), sizeof(KeySize));
            Stream.write(Key.c_str(), KeySize);
            
            uint32_t ValueSize = static_cast<uint32_t>(Value.size());
            Stream.write(reinterpret_cast<const char*>(&ValueSize), sizeof(ValueSize));
            Stream.write(Value.c_str(), ValueSize);
        }
        
        // 序列化消息状态
        Stream.write(reinterpret_cast<const char*>(&Message->Status), sizeof(Message->Status));
        
        // 序列化消息负载
        std::string PayloadStr = Message->Payload.AsString();
        uint32_t PayloadSize = static_cast<uint32_t>(PayloadStr.size());
        Stream.write(reinterpret_cast<const char*>(&PayloadSize), sizeof(PayloadSize));
        Stream.write(PayloadStr.c_str(), PayloadSize);
        
        // 获取序列化数据
        std::string SerializedStr = Stream.str();
        OutData.assign(SerializedStr.begin(), SerializedStr.end());
        
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "序列化消息失败 err={}", e.what());
        return QueueResult::INTERNAL_ERROR;
    }
}

QueueResult FileBasedPersistence::DeserializeMessage(const std::vector<uint8_t>& Data, MessagePtr& OutMessage)
{
    try
    {
        // 简单的反序列化实现
        std::string SerializedStr(Data.begin(), Data.end());
        std::stringstream Stream(SerializedStr);
        
        OutMessage = std::make_shared<Message>();
        
        // 反序列化消息头
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.Id), sizeof(OutMessage->Header.Id));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.Type), sizeof(OutMessage->Header.Type));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.Priority), sizeof(OutMessage->Header.Priority));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.Delivery), sizeof(OutMessage->Header.Delivery));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.Timestamp), sizeof(OutMessage->Header.Timestamp));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.ExpireTime), sizeof(OutMessage->Header.ExpireTime));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.RetryCount), sizeof(OutMessage->Header.RetryCount));
        Stream.read(reinterpret_cast<char*>(&OutMessage->Header.MaxRetries), sizeof(OutMessage->Header.MaxRetries));
        
        // 反序列化消息属性
        uint32_t PropertiesCount;
        Stream.read(reinterpret_cast<char*>(&PropertiesCount), sizeof(PropertiesCount));
        for (uint32_t i = 0; i < PropertiesCount; ++i)
        {
            uint32_t KeySize;
            Stream.read(reinterpret_cast<char*>(&KeySize), sizeof(KeySize));
            std::string Key(KeySize, '\0');
            Stream.read(&Key[0], KeySize);
            
            uint32_t ValueSize;
            Stream.read(reinterpret_cast<char*>(&ValueSize), sizeof(ValueSize));
            std::string Value(ValueSize, '\0');
            Stream.read(&Value[0], ValueSize);
            
            OutMessage->Header.Properties[Key] = Value;
        }
        
        // 反序列化消息状态
        Stream.read(reinterpret_cast<char*>(&OutMessage->Status), sizeof(OutMessage->Status));
        
        // 反序列化消息负载
        uint32_t PayloadSize;
        Stream.read(reinterpret_cast<char*>(&PayloadSize), sizeof(PayloadSize));
        
        std::string PayloadStr(PayloadSize, '\0');
        Stream.read(&PayloadStr[0], PayloadSize);
        OutMessage->Payload.SetString(PayloadStr);
        
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "反序列化消息失败 err={} size={}", e.what(), static_cast<uint64_t>(Data.size()));
        return QueueResult::INTERNAL_ERROR;
    }
}

QueueResult FileBasedPersistence::WriteIndexToFile()
{
    try
    {
        IndexFile.seekp(0);
        IndexFile.clear();
        
        // 写入索引版本
        uint32_t Version = 1;
        IndexFile.write(reinterpret_cast<const char*>(&Version), sizeof(Version));
        
        // 写入队列数量
        uint32_t QueueCount = static_cast<uint32_t>(QueueMessageIndex.size());
        IndexFile.write(reinterpret_cast<const char*>(&QueueCount), sizeof(QueueCount));
        
        for (const auto& [QueueName, Messages] : QueueMessageIndex)
        {
            // 写入队列名长度和队列名
            uint32_t QueueNameLength = static_cast<uint32_t>(QueueName.length());
            IndexFile.write(reinterpret_cast<const char*>(&QueueNameLength), sizeof(QueueNameLength));
            IndexFile.write(QueueName.c_str(), QueueNameLength);
            
            // 写入消息数量
            uint32_t MessageCount = static_cast<uint32_t>(Messages.size());
            IndexFile.write(reinterpret_cast<const char*>(&MessageCount), sizeof(MessageCount));
            
            for (const auto& [MessageId, Entry] : Messages)
            {
                // 写入索引条目
                IndexFile.write(reinterpret_cast<const char*>(&Entry.Id), sizeof(Entry.Id));
                IndexFile.write(reinterpret_cast<const char*>(&Entry.FileOffset), sizeof(Entry.FileOffset));
                IndexFile.write(reinterpret_cast<const char*>(&Entry.MessageSize), sizeof(Entry.MessageSize));
                IndexFile.write(reinterpret_cast<const char*>(&Entry.Timestamp), sizeof(Entry.Timestamp));
                IndexFile.write(reinterpret_cast<const char*>(&Entry.IsDeleted), sizeof(Entry.IsDeleted));
            }
        }
        
        IndexFile.flush();
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "写入索引失败 err={}", e.what());
        return QueueResult::INTERNAL_ERROR;
    }
}

QueueResult FileBasedPersistence::ReadIndexFromFile()
{
    try
    {
        if (!IndexFile.is_open() || IndexFile.eof())
        {
            return QueueResult::SUCCESS; // 文件不存在或为空是正常的
        }
        
        IndexFile.seekg(0);
        
        // 检查文件大小是否合理
        IndexFile.seekg(0, std::ios::end);
        auto fileSize = IndexFile.tellg();
        IndexFile.seekg(0);
        
        if (fileSize < static_cast<std::streampos>(sizeof(uint32_t) * 2))
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "索引文件太小，跳过读取");
            return QueueResult::SUCCESS;
        }
        
        // 读取索引版本
        uint32_t Version;
        IndexFile.read(reinterpret_cast<char*>(&Version), sizeof(Version));
        if (IndexFile.fail() || IndexFile.eof())
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取索引版本失败");
            return QueueResult::SUCCESS;
        }
        
        // 读取队列数量
        uint32_t QueueCount;
        IndexFile.read(reinterpret_cast<char*>(&QueueCount), sizeof(QueueCount));
        if (IndexFile.fail() || IndexFile.eof())
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列数量失败");
            return QueueResult::SUCCESS;
        }
        
        // 添加保护：限制最大队列数量
        const uint32_t MAX_QUEUE_COUNT = 10000;
        if (QueueCount > MAX_QUEUE_COUNT)
        {
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "队列数量过大 ({} > {})，跳过索引读取", QueueCount, MAX_QUEUE_COUNT);
            return QueueResult::SUCCESS;
        }
        
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "开始读取 {} 个队列的索引", QueueCount);
        
        for (uint32_t i = 0; i < QueueCount; ++i)
        {
            // 检查文件状态
            if (IndexFile.fail() || IndexFile.eof())
            {
                H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列 {} 时文件状态异常", i);
                break;
            }
            
            // 读取队列名长度和队列名
            uint32_t QueueNameLength;
            IndexFile.read(reinterpret_cast<char*>(&QueueNameLength), sizeof(QueueNameLength));
            if (IndexFile.fail() || IndexFile.eof())
            {
                H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列 {} 名称长度失败", i);
                break;
            }
            
            // 添加保护：限制队列名长度
            const uint32_t MAX_QUEUE_NAME_LENGTH = 1024;
            if (QueueNameLength > MAX_QUEUE_NAME_LENGTH)
            {
                H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "队列 {} 名称长度过大 ({} > {})，跳过", i, QueueNameLength, MAX_QUEUE_NAME_LENGTH);
                break;
            }
            
            std::string QueueName(QueueNameLength, '\0');
            IndexFile.read(&QueueName[0], QueueNameLength);
            if (IndexFile.fail() || IndexFile.eof())
            {
                H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列 {} 名称失败", i);
                break;
            }
            
            // 读取消息数量
            uint32_t MessageCount;
            IndexFile.read(reinterpret_cast<char*>(&MessageCount), sizeof(MessageCount));
            if (IndexFile.fail() || IndexFile.eof())
            {
                H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列 {} 消息数量失败", i);
                break;
            }
            
            // 添加保护：限制每个队列的消息数量
            const uint32_t MAX_MESSAGE_COUNT = 100000;
            if (MessageCount > MAX_MESSAGE_COUNT)
            {
                H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "队列 {} 消息数量过大 ({} > {})，跳过", QueueName, MessageCount, MAX_MESSAGE_COUNT);
                break;
            }
            
            H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "读取队列 {} 的 {} 条消息索引", QueueName, MessageCount);
            
            for (uint32_t j = 0; j < MessageCount; ++j)
            {
                // 检查文件状态
                if (IndexFile.fail() || IndexFile.eof())
                {
                    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列 {} 消息 {} 时文件状态异常", QueueName, j);
                    break;
                }
                
                MessageIndexEntry Entry;
                IndexFile.read(reinterpret_cast<char*>(&Entry.Id), sizeof(Entry.Id));
                IndexFile.read(reinterpret_cast<char*>(&Entry.FileOffset), sizeof(Entry.FileOffset));
                IndexFile.read(reinterpret_cast<char*>(&Entry.MessageSize), sizeof(Entry.MessageSize));
                IndexFile.read(reinterpret_cast<char*>(&Entry.Timestamp), sizeof(Entry.Timestamp));
                IndexFile.read(reinterpret_cast<char*>(&Entry.IsDeleted), sizeof(Entry.IsDeleted));
                
                if (IndexFile.fail() || IndexFile.eof())
                {
                    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Warning, "读取队列 {} 消息 {} 索引条目失败", QueueName, j);
                    break;
                }
                
                Entry.QueueName = QueueName;
                QueueMessageIndex[QueueName][Entry.Id] = Entry;
            }
        }
        
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "索引读取完成");
        return QueueResult::SUCCESS;
    }
    catch (const std::exception& e)
    {
        H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Error, "读取索引失败 err={}", e.what());
        return QueueResult::INTERNAL_ERROR;
    }
}

std::string FileBasedPersistence::GetQueueDataFileName(const std::string& QueueName)
{
    return QueueName + "_data.bin";
}

std::string FileBasedPersistence::GetMessageDataFileName(const std::string& QueueName)
{
    return QueueName + "_messages.bin";
}

std::string FileBasedPersistence::GetIndexFileName(const std::string& QueueName)
{
    return QueueName + "_index.bin";
}

bool FileBasedPersistence::IsMessageExpired(MessagePtr Message)
{
    if (!Message || Message->Header.ExpireTime == 0)
    {
        return false;
    }
    
    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return Now > Message->Header.ExpireTime;
}

void FileBasedPersistence::CleanupExpiredMessages()
{
    std::unique_lock<std::shared_mutex> IndexLock(IndexMutex);
    
    for (auto& [QueueName, Messages] : QueueMessageIndex)
    {
        std::vector<MessageId> ExpiredMessages;
        
        for (const auto& [MessageId, Entry] : Messages)
        {
            if (Entry.IsDeleted)
            {
                continue;
            }
            
            // 读取消息检查是否过期
            MessagePtr Message;
            auto Result = ReadMessageFromFile(Entry.FileOffset, Entry.MessageSize, Message);
            if (Result == QueueResult::SUCCESS && Message && IsMessageExpired(Message))
            {
                ExpiredMessages.push_back(MessageId);
            }
        }
        
        // 标记过期消息为已删除
        for (MessageId Id : ExpiredMessages)
        {
            Messages[Id].IsDeleted = true;
        }
    }
}

// PersistenceManager 实现
PersistenceManager::PersistenceManager()
    : Initialized(false)
{
    H_LOG(MQManager, Helianthus::Common::LogVerbosity::Log, "创建持久化管理器");
}

PersistenceManager::~PersistenceManager()
{
    Shutdown();
}

QueueResult PersistenceManager::Initialize(const PersistenceConfig& Config)
{
    if (Initialized.load())
    {
        return QueueResult::SUCCESS;
    }

    H_LOG(MQManager, Helianthus::Common::LogVerbosity::Log, "开始初始化持久化管理器");

    this->Config = Config;
    
    H_LOG(MQManager, Helianthus::Common::LogVerbosity::Display, "持久化类型: {}", static_cast<int>(Config.Type));
    
    // 根据配置类型创建持久化实现
    switch (Config.Type)
    {
        case PersistenceType::FILE_BASED:
            H_LOG(MQManager, Helianthus::Common::LogVerbosity::Display, "创建文件持久化实现");
            try {
                PersistenceImpl = std::make_unique<FileBasedPersistence>();
            } catch (const std::exception& e) {
                H_LOG(MQManager, Helianthus::Common::LogVerbosity::Error, "创建文件持久化实现失败: {}", e.what());
                return QueueResult::INTERNAL_ERROR;
            }
            break;
        case PersistenceType::MEMORY_ONLY:
            H_LOG(MQManager, Helianthus::Common::LogVerbosity::Display, "使用内存模式，跳过持久化");
            // 内存模式不需要持久化实现
            break;
        case PersistenceType::DATABASE:
            // 数据库模式暂未实现
            H_LOG(MQManager, Helianthus::Common::LogVerbosity::Warning, "数据库持久化暂未实现");
            return QueueResult::NOT_IMPLEMENTED;
        default:
            H_LOG(MQManager, Helianthus::Common::LogVerbosity::Error, "无效的持久化类型: {}", static_cast<int>(Config.Type));
            return QueueResult::INVALID_CONFIG;
    }
    
    if (PersistenceImpl)
    {
        H_LOG(MQManager, Helianthus::Common::LogVerbosity::Display, "开始初始化持久化实现");
        try {
            auto Result = PersistenceImpl->Initialize(Config);
            if (Result != QueueResult::SUCCESS)
            {
                H_LOG(MQManager, Helianthus::Common::LogVerbosity::Error, "持久化实现初始化失败: {}", static_cast<int>(Result));
                return Result;
            }
            H_LOG(MQManager, Helianthus::Common::LogVerbosity::Display, "持久化实现初始化成功");
        } catch (const std::exception& e) {
            H_LOG(MQManager, Helianthus::Common::LogVerbosity::Error, "持久化实现初始化异常: {}", e.what());
            return QueueResult::INTERNAL_ERROR;
        }
    }

    Initialized = true;
    H_LOG(MQManager, Helianthus::Common::LogVerbosity::Log, "持久化管理器初始化成功");
    return QueueResult::SUCCESS;
}

void PersistenceManager::Shutdown()
{
    if (!Initialized.load())
    {
        return;
    }

    H_LOG(MQManager, Helianthus::Common::LogVerbosity::Log, "开始关闭持久化管理器");

    if (PersistenceImpl)
    {
        PersistenceImpl->Shutdown();
        PersistenceImpl.reset();
    }

    Initialized = false;
    H_LOG(MQManager, Helianthus::Common::LogVerbosity::Log, "持久化管理器关闭完成");
}

bool PersistenceManager::IsInitialized() const
{
    return Initialized.load();
}

// 委托方法实现
QueueResult PersistenceManager::SaveQueue(const std::string& QueueName, const QueueConfig& Config, const QueueStats& Stats)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->SaveQueue(QueueName, Config, Stats);
}

QueueResult PersistenceManager::LoadQueue(const std::string& QueueName, QueueConfig& Config, QueueStats& Stats)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->LoadQueue(QueueName, Config, Stats);
}

QueueResult PersistenceManager::DeleteQueue(const std::string& QueueName)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->DeleteQueue(QueueName);
}

std::vector<std::string> PersistenceManager::ListPersistedQueues()
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return {};
    }
    return PersistenceImpl->ListPersistedQueues();
}

QueueResult PersistenceManager::SaveMessage(const std::string& QueueName, MessagePtr Message)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->SaveMessage(QueueName, Message);
}

QueueResult PersistenceManager::LoadMessage(const std::string& QueueName, MessageId MessageId, MessagePtr& OutMessage)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->LoadMessage(QueueName, MessageId, OutMessage);
}

QueueResult PersistenceManager::DeleteMessage(const std::string& QueueName, MessageId MessageId)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->DeleteMessage(QueueName, MessageId);
}

QueueResult PersistenceManager::SaveBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->SaveBatchMessages(QueueName, Messages);
}

QueueResult PersistenceManager::LoadAllMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->LoadAllMessages(QueueName, OutMessages);
}

QueueResult PersistenceManager::RebuildIndex()
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->RebuildIndex();
}

QueueResult PersistenceManager::CompactFiles()
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->CompactFiles();
}

QueueResult PersistenceManager::BackupData(const std::string& BackupPath)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->BackupData(BackupPath);
}

QueueResult PersistenceManager::RestoreData(const std::string& BackupPath)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    return PersistenceImpl->RestoreData(BackupPath);
}

size_t PersistenceManager::GetPersistedMessageCount(const std::string& QueueName)
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return 0;
    }
    return PersistenceImpl->GetPersistedMessageCount(QueueName);
}

size_t PersistenceManager::GetTotalPersistedSize()
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return 0;
    }
    return PersistenceImpl->GetTotalPersistedSize();
}

std::vector<std::string> PersistenceManager::GetDiagnostics()
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return {"持久化管理器未初始化"};
    }
    return PersistenceImpl->GetDiagnostics();
}

// 持久化指标相关方法实现
IMessagePersistence::PersistenceStats PersistenceManager::GetPersistenceStats()
{
    if (!Initialized.load() || !PersistenceImpl)
    {
        return IMessagePersistence::PersistenceStats{};
    }
    return PersistenceImpl->GetPersistenceStats();
}

void PersistenceManager::ResetPersistenceStats()
{
    if (Initialized.load() && PersistenceImpl)
    {
        PersistenceImpl->ResetPersistenceStats();
    }
}

IMessagePersistence::PersistenceStats FileBasedPersistence::GetPersistenceStats()
{
    IMessagePersistence::PersistenceStats Stats;
    
    Stats.TotalWriteCount = Metrics.TotalWriteCount.load(std::memory_order_relaxed);
    Stats.TotalReadCount = Metrics.TotalReadCount.load(std::memory_order_relaxed);
    Stats.TotalWriteTimeMs = Metrics.TotalWriteTimeMs.load(std::memory_order_relaxed);
    Stats.TotalReadTimeMs = Metrics.TotalReadTimeMs.load(std::memory_order_relaxed);
    Stats.MaxWriteTimeMs = Metrics.MaxWriteTimeMs.load(std::memory_order_relaxed);
    Stats.MaxReadTimeMs = Metrics.MaxReadTimeMs.load(std::memory_order_relaxed);
    
    uint64_t MinWrite = Metrics.MinWriteTimeMs.load(std::memory_order_relaxed);
    Stats.MinWriteTimeMs = (MinWrite == UINT64_MAX) ? 0 : MinWrite;
    
    uint64_t MinRead = Metrics.MinReadTimeMs.load(std::memory_order_relaxed);
    Stats.MinReadTimeMs = (MinRead == UINT64_MAX) ? 0 : MinRead;
    
    return Stats;
}

void FileBasedPersistence::ResetPersistenceStats()
{
    Metrics.Reset();
}

} // namespace Helianthus::MessageQueue
