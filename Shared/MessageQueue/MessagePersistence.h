#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <atomic>

#include "MessageTypes.h"
#include "Common/LogCategory.h"

namespace Helianthus::MessageQueue
{

/**
 * @brief 持久化模式枚举
 */
enum class PersistenceType
{
    MEMORY_ONLY,    // 仅内存，不持久化
    FILE_BASED,     // 基于文件存储
    DATABASE        // 数据库存储（预留）
};

/**
 * @brief 持久化配置
 */
struct PersistenceConfig
{
    PersistenceType Type = PersistenceType::MEMORY_ONLY;
    std::string DataDirectory = "./message_queue_data";
    std::string QueueDataFile = "queue_data.bin";
    std::string MessageDataFile = "messages.bin";
    std::string IndexFile = "index.bin";
    size_t MaxFileSize = 100 * 1024 * 1024;  // 100MB
    size_t MaxFiles = 10;
    bool EnableCompression = false;
    bool EnableEncryption = false;
    std::string EncryptionKey;
};

/**
 * @brief 消息索引条目
 */
struct MessageIndexEntry
{
    MessageId Id;
    std::string QueueName;
    size_t FileOffset;
    size_t MessageSize;
    MessageTimestamp Timestamp;
    bool IsDeleted;
    
    MessageIndexEntry() : Id(0), FileOffset(0), MessageSize(0), Timestamp(0), IsDeleted(false) {}
};

/**
 * @brief 队列持久化数据
 */
struct QueuePersistenceData
{
    std::string QueueName;
    QueueConfig Config;
    std::vector<MessageId> MessageIds;
    QueueStats Stats;
    bool IsDirty;
    
    QueuePersistenceData() : IsDirty(false) {}
};

/**
 * @brief 消息持久化接口
 */
class IMessagePersistence
{
public:
    virtual ~IMessagePersistence() = default;
    
    virtual QueueResult Initialize(const PersistenceConfig& Config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    
    // 队列持久化
    virtual QueueResult SaveQueue(const std::string& QueueName, const QueueConfig& Config, const QueueStats& Stats) = 0;
    virtual QueueResult LoadQueue(const std::string& QueueName, QueueConfig& Config, QueueStats& Stats) = 0;
    virtual QueueResult DeleteQueue(const std::string& QueueName) = 0;
    virtual std::vector<std::string> ListPersistedQueues() = 0;
    
    // 消息持久化
    virtual QueueResult SaveMessage(const std::string& QueueName, MessagePtr Message) = 0;
    virtual QueueResult LoadMessage(const std::string& QueueName, MessageId MessageId, MessagePtr& OutMessage) = 0;
    virtual QueueResult DeleteMessage(const std::string& QueueName, MessageId MessageId) = 0;
    virtual QueueResult SaveBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages) = 0;
    virtual QueueResult LoadAllMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages) = 0;
    
    // 索引管理
    virtual QueueResult RebuildIndex() = 0;
    virtual QueueResult CompactFiles() = 0;
    virtual QueueResult BackupData(const std::string& BackupPath) = 0;
    virtual QueueResult RestoreData(const std::string& BackupPath) = 0;
    
    // 统计信息
    virtual size_t GetPersistedMessageCount(const std::string& QueueName) = 0;
    virtual size_t GetTotalPersistedSize() = 0;
    virtual std::vector<std::string> GetDiagnostics() = 0;
    
    // 持久化耗时指标
    struct PersistenceStats
    {
        uint64_t TotalWriteCount = 0;
        uint64_t TotalReadCount = 0;
        uint64_t TotalWriteTimeMs = 0;
        uint64_t TotalReadTimeMs = 0;
        uint64_t MaxWriteTimeMs = 0;
        uint64_t MaxReadTimeMs = 0;
        uint64_t MinWriteTimeMs = UINT64_MAX;
        uint64_t MinReadTimeMs = UINT64_MAX;
        
        double GetAverageWriteTimeMs() const 
        { 
            return TotalWriteCount > 0 ? static_cast<double>(TotalWriteTimeMs) / TotalWriteCount : 0.0; 
        }
        
        double GetAverageReadTimeMs() const 
        { 
            return TotalReadCount > 0 ? static_cast<double>(TotalReadTimeMs) / TotalReadCount : 0.0; 
        }
    };
    
    virtual PersistenceStats GetPersistenceStats() = 0;
    virtual void ResetPersistenceStats() = 0;
};

/**
 * @brief 基于文件的持久化实现
 */
class FileBasedPersistence : public IMessagePersistence
{
public:
    FileBasedPersistence();
    ~FileBasedPersistence() override;
    
    // IMessagePersistence 接口实现
    QueueResult Initialize(const PersistenceConfig& Config) override;
    void Shutdown() override;
    bool IsInitialized() const override;
    
    QueueResult SaveQueue(const std::string& QueueName, const QueueConfig& Config, const QueueStats& Stats) override;
    QueueResult LoadQueue(const std::string& QueueName, QueueConfig& Config, QueueStats& Stats) override;
    QueueResult DeleteQueue(const std::string& QueueName) override;
    std::vector<std::string> ListPersistedQueues() override;
    
    QueueResult SaveMessage(const std::string& QueueName, MessagePtr Message) override;
    QueueResult LoadMessage(const std::string& QueueName, MessageId MessageId, MessagePtr& OutMessage) override;
    QueueResult DeleteMessage(const std::string& QueueName, MessageId MessageId) override;
    QueueResult SaveBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages) override;
    QueueResult LoadAllMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages) override;
    
    QueueResult RebuildIndex() override;
    QueueResult CompactFiles() override;
    QueueResult BackupData(const std::string& BackupPath) override;
    QueueResult RestoreData(const std::string& BackupPath) override;
    
    size_t GetPersistedMessageCount(const std::string& QueueName) override;
    size_t GetTotalPersistedSize() override;
    std::vector<std::string> GetDiagnostics() override;
    
    // 持久化耗时指标
    IMessagePersistence::PersistenceStats GetPersistenceStats() override;
    void ResetPersistenceStats() override;

private:
    // 内部状态
    PersistenceConfig Config;
    std::atomic<bool> Initialized;
    std::filesystem::path DataDir;
    std::filesystem::path QueueDataPath;
    std::filesystem::path MessageDataPath;
    std::filesystem::path IndexPath;
    
    // 文件流
    std::fstream QueueDataFile;
    std::fstream MessageDataFile;
    std::fstream IndexFile;
    
    // 索引缓存
    std::unordered_map<std::string, std::unordered_map<MessageId, MessageIndexEntry>> QueueMessageIndex;
    std::unordered_map<std::string, QueuePersistenceData> QueueData;
    
    // 同步
    mutable std::shared_mutex IndexMutex;
    mutable std::shared_mutex QueueDataMutex;
    std::mutex FileMutex;
    
    // 持久化耗时统计
    struct PersistenceMetrics
    {
        std::atomic<uint64_t> TotalWriteCount{0};
        std::atomic<uint64_t> TotalReadCount{0};
        std::atomic<uint64_t> TotalWriteTimeMs{0};
        std::atomic<uint64_t> TotalReadTimeMs{0};
        std::atomic<uint64_t> MaxWriteTimeMs{0};
        std::atomic<uint64_t> MaxReadTimeMs{0};
        std::atomic<uint64_t> MinWriteTimeMs{UINT64_MAX};
        std::atomic<uint64_t> MinReadTimeMs{UINT64_MAX};
        
        void RecordWriteTime(uint64_t TimeMs)
        {
            TotalWriteCount.fetch_add(1, std::memory_order_relaxed);
            TotalWriteTimeMs.fetch_add(TimeMs, std::memory_order_relaxed);
            
            uint64_t CurrentMax = MaxWriteTimeMs.load(std::memory_order_relaxed);
            while (TimeMs > CurrentMax && 
                   !MaxWriteTimeMs.compare_exchange_weak(CurrentMax, TimeMs, std::memory_order_relaxed));
            
            uint64_t CurrentMin = MinWriteTimeMs.load(std::memory_order_relaxed);
            while (TimeMs < CurrentMin && 
                   !MinWriteTimeMs.compare_exchange_weak(CurrentMin, TimeMs, std::memory_order_relaxed));
        }
        
        void RecordReadTime(uint64_t TimeMs)
        {
            TotalReadCount.fetch_add(1, std::memory_order_relaxed);
            TotalReadTimeMs.fetch_add(TimeMs, std::memory_order_relaxed);
            
            uint64_t CurrentMax = MaxReadTimeMs.load(std::memory_order_relaxed);
            while (TimeMs > CurrentMax && 
                   !MaxReadTimeMs.compare_exchange_weak(CurrentMax, TimeMs, std::memory_order_relaxed));
            
            uint64_t CurrentMin = MinReadTimeMs.load(std::memory_order_relaxed);
            while (TimeMs < CurrentMin && 
                   !MinReadTimeMs.compare_exchange_weak(CurrentMin, TimeMs, std::memory_order_relaxed));
        }
        
        void Reset()
        {
            TotalWriteCount.store(0, std::memory_order_relaxed);
            TotalReadCount.store(0, std::memory_order_relaxed);
            TotalWriteTimeMs.store(0, std::memory_order_relaxed);
            TotalReadTimeMs.store(0, std::memory_order_relaxed);
            MaxWriteTimeMs.store(0, std::memory_order_relaxed);
            MaxReadTimeMs.store(0, std::memory_order_relaxed);
            MinWriteTimeMs.store(UINT64_MAX, std::memory_order_relaxed);
            MinReadTimeMs.store(UINT64_MAX, std::memory_order_relaxed);
        }
    };
    
    PersistenceMetrics Metrics;
    std::mutex MetricsMutex;
    
    // 内部方法
    QueueResult EnsureDataDirectory();
    QueueResult OpenFiles();
    void CloseFiles();
    
    QueueResult WriteMessageToFile(MessagePtr Message, size_t& OutOffset);
    QueueResult ReadMessageFromFile(size_t Offset, size_t Size, MessagePtr& OutMessage);
    
    QueueResult UpdateMessageIndex(const std::string& QueueName, MessageId MessageId, 
                                  size_t FileOffset, size_t MessageSize);
    QueueResult RemoveMessageFromIndex(const std::string& QueueName, MessageId MessageId);
    
    QueueResult SerializeMessage(MessagePtr Message, std::vector<uint8_t>& OutData);
    QueueResult DeserializeMessage(const std::vector<uint8_t>& Data, MessagePtr& OutMessage);
    
    QueueResult WriteIndexToFile();
    QueueResult ReadIndexFromFile();
    
    std::string GetQueueDataFileName(const std::string& QueueName);
    std::string GetMessageDataFileName(const std::string& QueueName);
    std::string GetIndexFileName(const std::string& QueueName);
    
    bool IsMessageExpired(MessagePtr Message);
    void CleanupExpiredMessages();
};

/**
 * @brief 持久化管理器
 */
class PersistenceManager
{
public:
    PersistenceManager();
    ~PersistenceManager();
    
    QueueResult Initialize(const PersistenceConfig& Config);
    void Shutdown();
    bool IsInitialized() const;
    
    // 委托给具体的持久化实现
    QueueResult SaveQueue(const std::string& QueueName, const QueueConfig& Config, const QueueStats& Stats);
    QueueResult LoadQueue(const std::string& QueueName, QueueConfig& Config, QueueStats& Stats);
    QueueResult DeleteQueue(const std::string& QueueName);
    std::vector<std::string> ListPersistedQueues();
    
    QueueResult SaveMessage(const std::string& QueueName, MessagePtr Message);
    QueueResult LoadMessage(const std::string& QueueName, MessageId MessageId, MessagePtr& OutMessage);
    QueueResult DeleteMessage(const std::string& QueueName, MessageId MessageId);
    QueueResult SaveBatchMessages(const std::string& QueueName, const std::vector<MessagePtr>& Messages);
    QueueResult LoadAllMessages(const std::string& QueueName, std::vector<MessagePtr>& OutMessages);
    
    QueueResult RebuildIndex();
    QueueResult CompactFiles();
    QueueResult BackupData(const std::string& BackupPath);
    QueueResult RestoreData(const std::string& BackupPath);
    
    size_t GetPersistedMessageCount(const std::string& QueueName);
    size_t GetTotalPersistedSize();
    std::vector<std::string> GetDiagnostics();
    
    // 持久化耗时指标
    IMessagePersistence::PersistenceStats GetPersistenceStats();
    void ResetPersistenceStats();

private:
    std::unique_ptr<IMessagePersistence> PersistenceImpl;
    PersistenceConfig Config;
    std::atomic<bool> Initialized;
};

} // namespace Helianthus::MessageQueue
