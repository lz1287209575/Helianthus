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
    INVALID_CONFIG = 16
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

    // 获取文本内容
    std::string AsString() const
    {
        return std::string(Data.begin(), Data.end());
    }

    // 获取二进制数据
    const char* GetData() const
    {
        return Data.data();
    }
    const void* GetVoidData() const
    {
        return static_cast<const void*>(Data.data());
    }

    bool IsEmpty() const
    {
        return Data.empty();
    }
    void Clear()
    {
        Data.clear();
        Size = 0;
    }
    
    // 获取数据大小
    size_t GetSize() const
    {
        return Data.size();
    }
    
    // 设置字符串内容
    void SetString(const std::string& Text)
    {
        Data.assign(Text.begin(), Text.end());
        Size = static_cast<MessageSize>(Text.size());
        ContentType = "text/plain";
        Encoding = "utf-8";
    }
    
    // 设置二进制数据
    void SetData(const void* NewData, size_t NewSize)
    {
        Data.assign(static_cast<const char*>(NewData), 
                   static_cast<const char*>(NewData) + NewSize);
        Size = static_cast<MessageSize>(NewSize);
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

}  // namespace Helianthus::MessageQueue