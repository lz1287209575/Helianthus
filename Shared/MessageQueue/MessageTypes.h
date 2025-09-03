#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Common/Types.h"

namespace Helianthus::MessageQueue
{
// 消息队列基础类型定义
using MessageId = uint64_t;
using QueueId = uint32_t;
using TopicId = uint32_t;
using ConsumerId = uint32_t;
using ProducerId = uint32_t;
using SubscriberId = uint32_t;
using MessageSize = uint32_t;
using MessageTimestamp = Common::TimestampMs;

// 无效ID常量
static constexpr MessageId InvalidMessageId = 0;
static constexpr QueueId InvalidQueueId = 0;
static constexpr TopicId InvalidTopicId = 0;
static constexpr ConsumerId InvalidConsumerId = 0;
static constexpr ProducerId InvalidProducerId = 0;
static constexpr SubscriberId InvalidSubscriberId = 0;

// 消息类型枚举
enum class MessageType : uint8_t
{
    // 基础消息类型
    UNKNOWN = 0,
    TEXT = 1,
    BINARY = 2,
    JSON = 3,

    // 游戏专用消息类型
    PLAYER_EVENT = 10,         // 玩家事件
    GAME_STATE = 11,           // 游戏状态
    CHAT_MESSAGE = 12,         // 聊天消息
    SYSTEM_NOTIFICATION = 13,  // 系统通知
    COMBAT_EVENT = 14,         // 战斗事件
    ECONOMY_EVENT = 15,        // 经济事件
    GUILD_EVENT = 16,          // 公会事件
    MATCH_EVENT = 17,          // 匹配事件

    // 系统消息类型
    HEARTBEAT = 20,         // 心跳消息
    HEALTH_CHECK = 21,      // 健康检查
    METRICS = 22,           // 指标数据
    LOG_ENTRY = 23,         // 日志条目
    CONFIG_UPDATE = 24,     // 配置更新
    SERVICE_DISCOVERY = 25  // 服务发现事件
};

// 消息优先级
enum class MessagePriority : uint8_t
{
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3,
    REALTIME = 4  // 实时消息，如战斗同步
};

// 消息传递模式
enum class DeliveryMode : uint8_t
{
    FIRE_AND_FORGET = 0,  // 发送后遗忘
    AT_LEAST_ONCE = 1,    // 至少一次
    AT_MOST_ONCE = 2,     // 最多一次
    EXACTLY_ONCE = 3      // 恰好一次
};

// 消息持久化选项
enum class PersistenceMode : uint8_t
{
    MEMORY_ONLY = 0,      // 仅内存
    DISK_PERSISTENT = 1,  // 磁盘持久化
    HYBRID = 2            // 混合模式
};

// 队列类型
enum class QueueType : uint8_t
{
    STANDARD = 0,     // 标准队列
    PRIORITY = 1,     // 优先级队列
    DELAY = 2,        // 延迟队列
    DEAD_LETTER = 3,  // 死信队列
    BROADCAST = 4     // 广播队列
};

// 死信队列原因
enum class DeadLetterReason : uint8_t
{
    EXPIRED = 0,           // 消息过期
    MAX_RETRIES_EXCEEDED = 1,  // 超过最大重试次数
    REJECTED = 2,          // 消费者拒收
    QUEUE_FULL = 3,        // 队列已满
    MESSAGE_TOO_LARGE = 4, // 消息过大
    INVALID_MESSAGE = 5,   // 无效消息
    CONSUMER_ERROR = 6,    // 消费者处理错误
    TIMEOUT = 7,           // 处理超时
    UNKNOWN = 255          // 未知原因
};

// 消息状态
enum class MessageStatus : uint8_t
{
    PENDING = 0,       // 待发送
    SENT = 1,          // 已发送
    DELIVERED = 2,     // 已投递
    ACKNOWLEDGED = 3,  // 已确认
    FAILED = 4,        // 发送失败
    EXPIRED = 5,       // 已过期
    DEAD_LETTER = 6    // 死信
};

// 消息队列操作结果
enum class QueueResult : uint8_t
{
    SUCCESS = 0,
    QUEUE_NOT_FOUND = 1,
    QUEUE_FULL = 2,
    MESSAGE_TOO_LARGE = 3,
    CONSUMER_NOT_FOUND = 4,
    PRODUCER_NOT_FOUND = 5,
    SUBSCRIPTION_NOT_FOUND = 6,
    PERMISSION_DENIED = 7,
    TIMEOUT = 8,
    SERIALIZATION_ERROR = 9,
    NETWORK_ERROR = 10,
    STORAGE_ERROR = 11,
    INVALID_PARAMETER = 12,
    INTERNAL_ERROR = 13,
    MESSAGE_NOT_FOUND = 14,
    NOT_IMPLEMENTED = 15,
    INVALID_CONFIG = 16,
    INVALID_STATE = 17,        // 状态不正确
    OPERATION_FAILED = 18,     // 操作失败
    TRANSACTION_NOT_FOUND = 19, // 事务不存在
    CONSUMER_LIMIT_EXCEEDED = 20
};

// 消息头信息
struct MessageHeader
{
    MessageId Id = InvalidMessageId;
    MessageType Type = MessageType::UNKNOWN;
    MessagePriority Priority = MessagePriority::NORMAL;
    DeliveryMode Delivery = DeliveryMode::AT_LEAST_ONCE;
    MessageTimestamp Timestamp = 0;
    MessageTimestamp ExpireTime = 0;
    uint32_t RetryCount = 0;
    uint32_t MaxRetries = 3;
    MessageTimestamp NextRetryTime = 0;     // 下次重试时间
    DeadLetterReason DeadLetterReasonValue = DeadLetterReason::UNKNOWN;  // 死信原因
    std::string OriginalQueue;              // 原始队列名称
    std::string SourceId;
    std::string TargetId;
    std::string CorrelationId;
    std::unordered_map<std::string, std::string> Properties;
};

// 消息内容
struct MessagePayload
{
    std::vector<char> Data;
    MessageSize Size = 0;
    // 零拷贝支持：外部缓冲区
    const void* ExternalData = nullptr;
    MessageSize ExternalSize = 0;
    bool ExternalOwned = false;
    std::function<void(const void*)> ExternalDeallocator; // 可为空
    std::string ContentType = "application/octet-stream";
    std::string Encoding = "binary";

    MessagePayload() = default;

    // 从字符串构造
    explicit MessagePayload(const std::string& Text)
        : Data(Text.begin(), Text.end()),
          Size(static_cast<MessageSize>(Text.size())),
          ContentType("text/plain"),
          Encoding("utf-8")
    {
    }

    // 从二进制数据构造
    MessagePayload(const void* Data, size_t Size)
        : Data(static_cast<const char*>(Data), static_cast<const char*>(Data) + Size),
          Size(static_cast<MessageSize>(Size))
    {
    }

    ~MessagePayload()
    {
        if (ExternalOwned && ExternalData && ExternalDeallocator)
        {
            try { ExternalDeallocator(ExternalData); } catch (...) {}
        }
    }

    // 获取文本内容
    std::string AsString() const
    {
        if (ExternalData && ExternalSize > 0)
        {
            return std::string(static_cast<const char*>(ExternalData), static_cast<const char*>(ExternalData) + ExternalSize);
        }
        return std::string(Data.begin(), Data.end());
    }

    // 获取二进制数据
    const char* GetData() const
    {
        if (ExternalData) return static_cast<const char*>(ExternalData);
        return Data.data();
    }
    const void* GetVoidData() const
    {
        if (ExternalData) return ExternalData;
        return static_cast<const void*>(Data.data());
    }

    bool IsEmpty() const
    {
        if (ExternalData) return ExternalSize == 0;
        return Data.empty();
    }
    void Clear()
    {
        Data.clear();
        Size = 0;
        // 清除外部数据引用，但不负责释放（如需释放应设 ExternalOwned=true 且提供释放器）
        ExternalData = nullptr;
        ExternalSize = 0;
        ExternalOwned = false;
        ExternalDeallocator = nullptr;
    }
    
    // 获取数据大小
    size_t GetSize() const
    {
        if (ExternalData) return ExternalSize;
        return Data.size();
    }
    
    // 设置字符串内容
    void SetString(const std::string& Text)
    {
        Data.assign(Text.begin(), Text.end());
        Size = static_cast<MessageSize>(Text.size());
        ContentType = "text/plain";
        Encoding = "utf-8";
        // 覆盖外部数据
        ExternalData = nullptr;
        ExternalSize = 0;
        ExternalOwned = false;
        ExternalDeallocator = nullptr;
    }
    
    // 设置二进制数据
    void SetData(const void* NewData, size_t NewSize)
    {
        Data.assign(static_cast<const char*>(NewData), 
                   static_cast<const char*>(NewData) + NewSize);
        Size = static_cast<MessageSize>(NewSize);
        ContentType = "application/octet-stream";
        Encoding = "binary";
        // 覆盖外部数据
        ExternalData = nullptr;
        ExternalSize = 0;
        ExternalOwned = false;
        ExternalDeallocator = nullptr;
    }

    // 设置外部零拷贝数据（不复制）
    void SetExternal(const void* Ptr, size_t NewSize, bool Owned = false, std::function<void(const void*)> Deallocator = nullptr)
    {
        ExternalData = Ptr;
        ExternalSize = static_cast<MessageSize>(NewSize);
        ExternalOwned = Owned;
        ExternalDeallocator = std::move(Deallocator);
        // 清空内部缓冲
        Data.clear();
        Size = ExternalSize;
        ContentType = "application/octet-stream";
        Encoding = "binary";
    }
};

// 完整消息结构
struct Message
{
    MessageHeader Header;
    MessagePayload Payload;
    MessageStatus Status = MessageStatus::PENDING;
    MessageTimestamp CreatedTime = 0;
    MessageTimestamp LastModifiedTime = 0;

    Message()
    {
        auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        Header.Timestamp = Now;
        CreatedTime = Now;
        LastModifiedTime = Now;
    }

    Message(MessageType Type, const std::string& Data) : Message()
    {
        Header.Type = Type;
        Payload = MessagePayload(Data);
    }

    Message(MessageType Type, const void* Data, size_t Size) : Message()
    {
        Header.Type = Type;
        Payload = MessagePayload(Data, Size);
    }

    bool IsExpired() const
    {
        if (Header.ExpireTime == 0)
            return false;
        auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        return static_cast<MessageTimestamp>(Now) > Header.ExpireTime;
    }

    bool CanRetry() const
    {
        return Header.RetryCount < Header.MaxRetries;
    }

    void IncrementRetry()
    {
        Header.RetryCount++;
        LastModifiedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    }
};

// 队列配置
struct QueueConfig
{
    std::string Name;
    QueueType Type = QueueType::STANDARD;
    PersistenceMode Persistence = PersistenceMode::MEMORY_ONLY;
    uint32_t MaxSize = 10000;                   // 最大消息数量
    uint64_t MaxSizeBytes = 100 * 1024 * 1024;  // 最大字节数 (100MB)
    uint32_t MaxConsumers = 100;                // 最大消费者数量
    uint32_t MaxProducers = 100;                // 最大生产者数量
    uint32_t MessageTtlMs = 300000;             // 消息TTL (5分钟)
    uint32_t QueueTtlMs = 0;                    // 队列TTL (0表示永不过期)
    bool EnableDeadLetter = true;               // 启用死信队列
    std::string DeadLetterQueue;                // 死信队列名称
    uint32_t MaxRetries = 3;                    // 最大重试次数
    uint32_t RetryDelayMs = 1000;               // 重试延迟（毫秒）
    bool EnableRetryBackoff = true;             // 启用指数退避
    double RetryBackoffMultiplier = 2.0;        // 退避倍数
    uint32_t MaxRetryDelayMs = 60000;           // 最大重试延迟（毫秒）
    uint32_t DeadLetterTtlMs = 86400000;        // 死信消息TTL（24小时）
    bool EnablePriority = false;                // 启用优先级排序
    bool EnableBatching = true;                 // 启用批量处理
    uint32_t BatchSize = 100;                   // 批量大小
    uint32_t BatchTimeoutMs = 1000;             // 批量超时
};

// 主题配置（发布订阅模式）
struct TopicConfig
{
    std::string Name;
    PersistenceMode Persistence = PersistenceMode::MEMORY_ONLY;
    uint32_t MaxSubscribers = 1000;                // 最大订阅者数量
    uint32_t MessageTtlMs = 60000;                 // 消息TTL (1分钟)
    uint32_t RetentionMs = 3600000;                // 消息保留时间 (1小时)
    uint64_t RetentionBytes = 1024 * 1024 * 1024;  // 保留字节数 (1GB)
    bool EnablePartitioning = false;               // 启用分区
    uint32_t PartitionCount = 1;                   // 分区数量
    std::vector<std::string> AllowedMessageTypes;  // 允许的消息类型
};

// 消费者配置
struct ConsumerConfig
{
    std::string ConsumerId;
    std::string GroupId;                                 // 消费者组ID
    bool AutoAcknowledge = true;                         // 自动确认
    uint32_t PrefetchCount = 10;                         // 预取消息数量
    uint32_t AckTimeoutMs = 30000;                       // 确认超时时间
    bool EnableBatching = false;                         // 启用批量消费
    uint32_t BatchSize = 10;                             // 批量大小
    uint32_t BatchTimeoutMs = 1000;                      // 批量超时
    MessagePriority MinPriority = MessagePriority::LOW;  // 最低优先级
};

// 生产者配置
struct ProducerConfig
{
    std::string ProducerId;
    bool EnableBatching = false;      // 启用批量发送
    uint32_t BatchSize = 100;         // 批量大小
    uint32_t BatchTimeoutMs = 1000;   // 批量超时
    bool WaitForAcknowledge = false;  // 等待确认
    uint32_t AckTimeoutMs = 5000;     // 确认超时
    uint32_t MaxRetries = 3;          // 最大重试次数
    uint32_t RetryIntervalMs = 1000;  // 重试间隔
};

// 统计信息
struct QueueStats
{
    uint64_t TotalMessages = 0;            // 总消息数
    uint64_t PendingMessages = 0;          // 待处理消息数
    uint64_t ProcessedMessages = 0;        // 已处理消息数
    uint64_t FailedMessages = 0;           // 失败消息数
    uint64_t DeadLetterMessages = 0;       // 死信消息数
    uint64_t RetriedMessages = 0;          // 重试消息数
    uint64_t ExpiredMessages = 0;          // 过期消息数
    uint64_t RejectedMessages = 0;         // 拒收消息数
    uint64_t TotalBytes = 0;               // 总字节数
    uint32_t ActiveConsumers = 0;          // 活跃消费者数
    uint32_t ActiveProducers = 0;          // 活跃生产者数
    uint32_t ActiveSubscribers = 0;        // 活跃订阅者数
    double AverageLatencyMs = 0.0;         // 平均延迟
    double ThroughputPerSecond = 0.0;      // 吞吐量
    MessageTimestamp LastMessageTime = 0;  // 最后消息时间
    MessageTimestamp CreatedTime = 0;      // 队列创建时间
};

// DLQ监控统计信息
struct DeadLetterQueueStats
{
    std::string QueueName;                    // 原队列名称
    std::string DeadLetterQueueName;          // 死信队列名称
    uint64_t TotalDeadLetterMessages = 0;     // 总死信消息数
    uint64_t CurrentDeadLetterMessages = 0;   // 当前死信队列中的消息数
    uint64_t ExpiredMessages = 0;             // 过期消息数
    uint64_t MaxRetriesExceededMessages = 0;  // 超过最大重试次数消息数
    uint64_t RejectedMessages = 0;            // 拒收消息数
    uint64_t QueueFullMessages = 0;           // 队列已满消息数
    uint64_t MessageTooLargeMessages = 0;     // 消息过大消息数
    uint64_t InvalidMessageMessages = 0;      // 无效消息数
    uint64_t ConsumerErrorMessages = 0;       // 消费者错误消息数
    uint64_t TimeoutMessages = 0;             // 超时消息数
    uint64_t UnknownReasonMessages = 0;       // 未知原因消息数
    MessageTimestamp LastDeadLetterTime = 0;  // 最后死信消息时间
    MessageTimestamp CreatedTime = 0;         // 死信队列创建时间
    double DeadLetterRate = 0.0;              // 死信率（死信消息数/总消息数）
};

// DLQ告警配置
struct DeadLetterAlertConfig
{
    uint64_t MaxDeadLetterMessages = 1000;    // 最大死信消息数阈值
    double MaxDeadLetterRate = 0.1;           // 最大死信率阈值（10%）
    uint32_t AlertCheckIntervalMs = 60000;    // 告警检查间隔（毫秒）
    bool EnableDeadLetterRateAlert = true;    // 启用死信率告警
    bool EnableDeadLetterCountAlert = true;   // 启用死信数量告警
    bool EnableDeadLetterTrendAlert = true;   // 启用死信趋势告警
};

// DLQ告警事件
enum class DeadLetterAlertType : uint8_t
{
    DEAD_LETTER_COUNT_EXCEEDED = 0,    // 死信数量超过阈值
    DEAD_LETTER_RATE_EXCEEDED = 1,     // 死信率超过阈值
    DEAD_LETTER_TREND_ANOMALY = 2,     // 死信趋势异常
    DEAD_LETTER_QUEUE_FULL = 3,        // 死信队列已满
    DEAD_LETTER_PROCESSING_FAILED = 4  // 死信处理失败
};

// DLQ告警信息
struct DeadLetterAlert
{
    DeadLetterAlertType Type;              // 告警类型
    std::string QueueName;                 // 队列名称
    std::string DeadLetterQueueName;       // 死信队列名称
    std::string AlertMessage;              // 告警消息
    uint64_t CurrentValue;                 // 当前值
    uint64_t ThresholdValue;               // 阈值
    double CurrentRate;                    // 当前比率
    double ThresholdRate;                  // 阈值比率
    MessageTimestamp AlertTime;            // 告警时间
    bool IsActive;                         // 是否活跃
};

// DLQ监控回调函数类型定义
using DeadLetterAlertHandler = std::function<void(const DeadLetterAlert& Alert)>;
using DeadLetterStatsHandler = std::function<void(const DeadLetterQueueStats& Stats)>;

// 前向声明
class IMessageQueue;
class IMessageConsumer;
class IMessageProducer;
class ITopicPublisher;
class ITopicSubscriber;

// 智能指针类型定义
using MessagePtr = std::shared_ptr<Message>;
using MessageQueuePtr = std::shared_ptr<IMessageQueue>;
using MessageConsumerPtr = std::shared_ptr<IMessageConsumer>;
using MessageProducerPtr = std::shared_ptr<IMessageProducer>;
using TopicPublisherPtr = std::shared_ptr<ITopicPublisher>;
using TopicSubscriberPtr = std::shared_ptr<ITopicSubscriber>;

// 回调函数类型定义
using MessageHandler = std::function<void(MessagePtr Message)>;
using BatchMessageHandler = std::function<void(const std::vector<MessagePtr>& Messages)>;
using ErrorHandler = std::function<void(QueueResult Result, const std::string& ErrorMessage)>;
using AcknowledgeHandler = std::function<void(MessageId MessageId, bool Success)>;
using QueueEventHandler = std::function<void(
    const std::string& QueueName, const std::string& Event, const std::string& Details)>;

// 集群/分片/副本 基础类型
using ShardId = uint32_t;

enum class ReplicaRole : uint8_t
{
    LEADER = 0,
    FOLLOWER = 1,
    CANDIDATE = 2,
    UNKNOWN = 255
};

struct ClusterNode
{
    std::string NodeId;           // 唯一节点ID
    std::string Host;             // 节点地址
    uint16_t Port = 0;            // 服务端口
    bool IsLocal = false;         // 是否为本地节点（单进程模拟时为true）
};

struct ReplicaInfo
{
    std::string NodeId;           // 副本所在节点
    ReplicaRole Role = ReplicaRole::FOLLOWER;
    bool Healthy = true;          // 健康状态（占位）
};

struct ShardInfo
{
    ShardId Id = 0;                               // 分片ID
    std::vector<ReplicaInfo> Replicas;            // 副本列表（第一个可约定为Leader）
};

// 分片分配与集群配置
struct ShardAssignment
{
    // 简化映射：队列名 -> 分片ID（后续可扩展为范围/一致性哈希）
    std::unordered_map<std::string, ShardId> QueueToShard;
};

struct ClusterConfig
{
    std::vector<ClusterNode> Nodes;               // 节点列表
    std::vector<ShardInfo> Shards;                // 分片与副本布局
    ShardAssignment Assignment;                   // 分配表
    uint32_t ReplicationFactor = 1;               // 副本因子（含Leader）
};

// HA回调
using LeaderChangeHandler = std::function<void(ShardId Shard, const std::string& OldLeader, const std::string& NewLeader)>;
using FailoverHandler = std::function<void(ShardId Shard, const std::string& FailedLeader, const std::string& TakeoverNode)>;

// 队列监控指标
struct QueueMetrics
{
    std::string QueueName;
    uint64_t PendingMessages = 0;      // 当前待处理消息数（队列长度）
    uint64_t TotalMessages = 0;        // 总入队数
    uint64_t ProcessedMessages = 0;    // 已处理(出队并确认)消息数
    uint64_t DeadLetterMessages = 0;   // 死信累计数
    uint64_t RetriedMessages = 0;      // 已重试累计数

    // 速率（条/秒），以窗口内差分估算
    double EnqueueRate = 0.0;          // 入队速率
    double DequeueRate = 0.0;          // 出队速率

    // 处理时延分位（占位，后续实现采样与TDigest）
    double P50LatencyMs = 0.0;
    double P95LatencyMs = 0.0;

    // 采样时间
    MessageTimestamp Timestamp = 0;
};

// 事务相关类型
using TransactionId = uint64_t;
using TransactionTimestamp = Common::TimestampMs;

// 事务状态枚举
enum class TransactionStatus : uint8_t
{
    PENDING = 0,        // 待处理
    COMMITTED = 1,      // 已提交
    ROLLED_BACK = 2,    // 已回滚
    TIMEOUT = 3,        // 超时
    FAILED = 4          // 失败
};

// 事务操作类型
enum class TransactionOperationType : uint8_t
{
    SEND_MESSAGE = 0,   // 发送消息
    ACKNOWLEDGE = 1,    // 确认消息
    REJECT_MESSAGE = 2, // 拒收消息
    CREATE_QUEUE = 3,   // 创建队列
    DELETE_QUEUE = 4    // 删除队列
};

// 事务操作
struct TransactionOperation
{
    TransactionOperationType Type = TransactionOperationType::SEND_MESSAGE;
    std::string QueueName;                    // 队列名称
    MessagePtr Message;                       // 消息（发送/确认/拒收时使用）
    MessageId TargetMessageId = InvalidMessageId;   // 消息ID（确认/拒收时使用）
    QueueConfig TargetQueueConfig;                  // 队列配置（创建队列时使用）
    std::string ErrorMessage;                 // 错误信息
    TransactionTimestamp Timestamp = 0;       // 操作时间戳
};

// 事务信息
struct Transaction
{
    TransactionId Id = 0;                     // 事务ID
    TransactionStatus Status = TransactionStatus::PENDING; // 事务状态
    std::vector<TransactionOperation> Operations; // 事务操作列表
    TransactionTimestamp StartTime = 0;       // 开始时间
    TransactionTimestamp EndTime = 0;         // 结束时间
    std::string Description;                  // 事务描述
    uint32_t TimeoutMs = 30000;              // 超时时间（毫秒）
    bool IsDistributed = false;              // 是否为分布式事务
    std::string CoordinatorId;               // 协调者ID（分布式事务）
};

// 事务统计
struct TransactionStats
{
    uint64_t TotalTransactions = 0;           // 总事务数
    uint64_t CommittedTransactions = 0;       // 已提交事务数
    uint64_t RolledBackTransactions = 0;      // 已回滚事务数
    uint64_t TimeoutTransactions = 0;         // 超时事务数
    uint64_t FailedTransactions = 0;          // 失败事务数
    
    // 成功率统计
    double SuccessRate = 0.0;                 // 成功率
    double RollbackRate = 0.0;                // 回滚率
    double TimeoutRate = 0.0;                 // 超时率
    
    // 性能统计
    double AverageCommitTimeMs = 0.0;         // 平均提交时间
    double AverageRollbackTimeMs = 0.0;       // 平均回滚时间
    TransactionTimestamp LastUpdateTime = 0;  // 最后更新时间
};

// 事务回调函数类型
using TransactionCommitHandler = std::function<void(TransactionId Id, bool Success, const std::string& ErrorMessage)>;
using TransactionRollbackHandler = std::function<void(TransactionId Id, const std::string& Reason)>;
using TransactionTimeoutHandler = std::function<void(TransactionId Id)>;

// 压缩算法枚举
enum class CompressionAlgorithm : uint8_t
{
    NONE = 0,           // 无压缩
    GZIP = 1,           // GZIP 压缩
    LZ4 = 2,            // LZ4 压缩
    ZSTD = 3,           // ZSTD 压缩
    SNAPPY = 4          // Snappy 压缩
};

// 加密算法枚举
enum class EncryptionAlgorithm : uint8_t
{
    NONE = 0,           // 无加密
    AES_256_GCM = 1,    // AES-256-GCM 加密
    CHACHA20_POLY1305 = 2, // ChaCha20-Poly1305 加密
    AES_128_CBC = 3     // AES-128-CBC 加密
};

// 压缩配置
struct CompressionConfig
{
    CompressionAlgorithm Algorithm = CompressionAlgorithm::NONE;
    uint32_t Level = 6;              // 压缩级别 (1-9, 越高压缩率越好但速度越慢)
    uint32_t MinSize = 1024;         // 最小压缩大小 (小于此大小的消息不压缩)
    bool EnableAutoCompression = true; // 是否启用自动压缩
};

// 加密配置
struct EncryptionConfig
{
    EncryptionAlgorithm Algorithm = EncryptionAlgorithm::NONE;
    std::string Key;                 // 加密密钥
    std::string IV;                  // 初始化向量 (某些算法需要)
    bool EnableAutoEncryption = true; // 是否启用自动加密
};

// 压缩统计
struct CompressionStats
{
    uint64_t TotalMessages = 0;           // 总消息数
    uint64_t CompressedMessages = 0;      // 已压缩消息数
    uint64_t OriginalBytes = 0;           // 原始字节数
    uint64_t CompressedBytes = 0;         // 压缩后字节数
    double CompressionRatio = 0.0;        // 压缩比
    double AverageCompressionTimeMs = 0.0; // 平均压缩时间
    double AverageDecompressionTimeMs = 0.0; // 平均解压时间
    Common::TimestampMs LastUpdateTime = 0; // 最后更新时间
};

// 加密统计
struct EncryptionStats
{
    uint64_t TotalMessages = 0;           // 总消息数
    uint64_t EncryptedMessages = 0;       // 已加密消息数
    double AverageEncryptionTimeMs = 0.0; // 平均加密时间
    double AverageDecryptionTimeMs = 0.0; // 平均解密时间
    Common::TimestampMs LastUpdateTime = 0; // 最后更新时间
};

// 告警级别枚举
enum class AlertLevel : uint8_t
{
    INFO = 0,        // 信息级别
    WARNING = 1,     // 警告级别
    ERROR = 2,       // 错误级别
    CRITICAL = 3     // 严重级别
};

// 告警类型枚举
enum class AlertType : uint8_t
{
    QUEUE_FULL = 0,              // 队列已满
    QUEUE_EMPTY = 1,             // 队列为空
    HIGH_LATENCY = 2,            // 高延迟
    LOW_THROUGHPUT = 3,          // 低吞吐量
    DEAD_LETTER_HIGH = 4,        // 死信队列消息过多
    CONSUMER_OFFLINE = 5,        // 消费者离线
    DISK_SPACE_LOW = 6,          // 磁盘空间不足
    MEMORY_USAGE_HIGH = 7,       // 内存使用率过高
    CPU_USAGE_HIGH = 8,          // CPU使用率过高
    NETWORK_ERROR = 9,           // 网络错误
    PERSISTENCE_ERROR = 10,      // 持久化错误
    COMPRESSION_ERROR = 11,      // 压缩错误
    ENCRYPTION_ERROR = 12,       // 加密错误
    TRANSACTION_TIMEOUT = 13,    // 事务超时
    REPLICATION_LAG = 14,        // 复制滞后
    NODE_HEALTH_DEGRADED = 15,   // 节点健康状态恶化
    CUSTOM = 16                  // 自定义告警
};

// 告警配置
struct AlertConfig
{
    AlertType Type = AlertType::QUEUE_FULL;
    AlertLevel Level = AlertLevel::WARNING;
    std::string QueueName;                    // 队列名称（如果适用）
    double Threshold = 0.0;                   // 阈值
    uint32_t DurationMs = 60000;              // 持续时间（毫秒）
    uint32_t CooldownMs = 300000;             // 冷却时间（毫秒）
    bool Enabled = true;                      // 是否启用
    std::string Description;                  // 告警描述
    std::vector<std::string> NotifyChannels;  // 通知渠道
};

// 告警信息
struct Alert
{
    uint64_t Id = 0;                          // 告警ID
    AlertType Type = AlertType::QUEUE_FULL;
    AlertLevel Level = AlertLevel::WARNING;
    std::string QueueName;                    // 队列名称
    std::string Message;                      // 告警消息
    double CurrentValue = 0.0;                // 当前值
    double Threshold = 0.0;                   // 阈值
    Common::TimestampMs TriggerTime = 0;      // 触发时间
    Common::TimestampMs LastUpdateTime = 0;   // 最后更新时间
    bool IsActive = false;                    // 是否活跃
    uint32_t OccurrenceCount = 0;             // 发生次数
    std::string Details;                      // 详细信息
};

// 告警统计
struct AlertStats
{
    uint64_t TotalAlerts = 0;                 // 总告警数
    uint64_t ActiveAlerts = 0;                // 活跃告警数
    uint64_t InfoAlerts = 0;                  // 信息级别告警数
    uint64_t WarningAlerts = 0;               // 警告级别告警数
    uint64_t ErrorAlerts = 0;                 // 错误级别告警数
    uint64_t CriticalAlerts = 0;              // 严重级别告警数
    double AverageResolutionTimeMs = 0.0;     // 平均解决时间
    Common::TimestampMs LastUpdateTime = 0;   // 最后更新时间
};

// 告警回调函数类型
using AlertHandler = std::function<void(const Alert& Alert)>;
using AlertConfigHandler = std::function<void(const AlertConfig& Config)>;

// 内存池配置
struct MemoryPoolConfig
{
    uint32_t InitialSize = 1024 * 1024;        // 初始大小 (1MB)
    uint32_t MaxSize = 100 * 1024 * 1024;      // 最大大小 (100MB)
    uint32_t BlockSize = 4096;                 // 块大小 (4KB)
    uint32_t GrowthFactor = 2;                 // 增长因子
    bool EnablePreallocation = true;           // 是否启用预分配
    uint32_t PreallocationBlocks = 1000;       // 预分配块数
    bool EnableCompaction = true;              // 是否启用内存压缩
    uint32_t CompactionThreshold = 50;         // 压缩阈值 (百分比)
};

// 缓冲区配置
struct BufferConfig
{
    uint32_t InitialCapacity = 8192;           // 初始容量 (8KB)
    uint32_t MaxCapacity = 1024 * 1024;        // 最大容量 (1MB)
    uint32_t GrowthFactor = 2;                 // 增长因子
    bool EnableZeroCopy = true;                // 是否启用零拷贝
    bool EnableCompression = false;            // 是否启用压缩
    uint32_t CompressionThreshold = 1024;      // 压缩阈值
    bool EnableBatching = true;                // 是否启用批处理
    uint32_t BatchSize = 100;                  // 批处理大小
    uint32_t BatchTimeoutMs = 100;             // 批处理超时时间
};

// 性能统计
struct PerformanceStats
{
    uint64_t TotalAllocations = 0;             // 总分配次数
    uint64_t TotalDeallocations = 0;           // 总释放次数
    uint64_t TotalBytesAllocated = 0;          // 总分配字节数
    uint64_t CurrentBytesAllocated = 0;        // 当前分配字节数
    uint64_t PeakBytesAllocated = 0;           // 峰值分配字节数
    uint64_t MemoryPoolHits = 0;               // 内存池命中次数
    uint64_t MemoryPoolMisses = 0;             // 内存池未命中次数
    double MemoryPoolHitRate = 0.0;            // 内存池命中率
    uint64_t ZeroCopyOperations = 0;           // 零拷贝操作次数
    uint64_t BatchOperations = 0;              // 批处理操作次数
    double AverageAllocationTimeMs = 0.0;      // 平均分配时间
    double AverageDeallocationTimeMs = 0.0;    // 平均释放时间
    double AverageZeroCopyTimeMs = 0.0;        // 平均零拷贝时间
    double AverageBatchTimeMs = 0.0;           // 平均批处理时间
    Common::TimestampMs LastUpdateTime = 0;    // 最后更新时间
};

// 批处理消息
struct BatchMessage
{
    std::vector<MessagePtr> Messages;          // 消息列表
    uint32_t BatchId = 0;                      // 批处理ID
    std::string QueueName;                     // 批处理目标队列
    Common::TimestampMs CreateTime = 0;        // 创建时间
    Common::TimestampMs ExpireTime = 0;        // 过期时间
    bool IsCompressed = false;                 // 是否已压缩
    uint32_t OriginalSize = 0;                 // 原始大小
    uint32_t CompressedSize = 0;               // 压缩后大小
};

// 零拷贝缓冲区
struct ZeroCopyBuffer
{
    void* Data = nullptr;                      // 数据指针
    size_t Size = 0;                           // 数据大小
    size_t Capacity = 0;                       // 缓冲区容量
    bool IsOwned = false;                      // 是否拥有内存
    std::function<void(void*)> Deallocator;    // 释放函数
};

// 内存池块
struct MemoryBlock
{
    void* Data = nullptr;                      // 数据指针
    size_t Size = 0;                           // 块大小
    bool IsUsed = false;                       // 是否已使用
    MemoryBlock* Next = nullptr;               // 下一个块
    Common::TimestampMs AllocTime = 0;         // 分配时间
};

}  // namespace Helianthus::MessageQueue