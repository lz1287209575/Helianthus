#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"
#include "Shared/Common/StructuredLogger.h"

using namespace Helianthus::MessageQueue;

// DLQ告警处理器
void OnDeadLetterAlert(const DeadLetterAlert& Alert)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, 
          "DLQ告警触发: queue={}, type={}, message={}, currentValue={}, thresholdValue={}, currentRate={:.2f}, thresholdRate={:.2f}", 
          Alert.QueueName, static_cast<int>(Alert.Type), Alert.AlertMessage, 
          Alert.CurrentValue, Alert.ThresholdValue, Alert.CurrentRate, Alert.ThresholdRate);
}

// DLQ统计处理器
void OnDeadLetterStats(const DeadLetterQueueStats& Stats)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
          "DLQ统计更新: queue={}, totalDLQ={}, currentDLQ={}, expired={}, rejected={}, rate={:.2f}%", 
          Stats.QueueName, Stats.TotalDeadLetterMessages, Stats.CurrentDeadLetterMessages,
          Stats.ExpiredMessages, Stats.RejectedMessages, Stats.DeadLetterRate * 100.0);
}

int main()
{
    // 初始化日志系统
    Helianthus::Common::Logger::LoggerConfig logCfg;
    logCfg.Level = Helianthus::Common::LogLevel::INFO;
    logCfg.EnableConsole = true;
    logCfg.EnableFile = false;
    logCfg.UseAsync = false;
    Helianthus::Common::Logger::Initialize(logCfg);

    // 初始化结构化日志
    Helianthus::Common::StructuredLoggerConfig structuredCfg;
    structuredCfg.MinLevel = Helianthus::Common::StructuredLogLevel::INFO;
    structuredCfg.EnableConsole = false;
    structuredCfg.EnableFile = true;
    structuredCfg.FilePath = "logs/structured.log";
    structuredCfg.MaxFileSize = 5 * 1024 * 1024; // 5MB
    structuredCfg.MaxFiles = 3;
    structuredCfg.EnableJsonOutput = true;
    structuredCfg.UseAsync = false;
    Helianthus::Common::StructuredLogger::Initialize(structuredCfg);
    // 设置全局字段（示例）
    Helianthus::Common::StructuredLogger::SetGlobalField("service", "dlq_monitor_test");
    Helianthus::Common::StructuredLogger::SetGlobalField("component", "MessageQueue");
    Helianthus::Common::StructuredLogger::SetGlobalField("env", "dev");

    // 设置线程本地上下文字段（示例）
    Helianthus::Common::StructuredLogger::SetThreadField("request_id", std::string("req-123456"));
    Helianthus::Common::StructuredLogger::SetThreadField("session_id", std::string("sess-abc"));
    Helianthus::Common::StructuredLogger::SetThreadField("user_id", std::string("user-42"));

    // 输出一条结构化日志验证
    Helianthus::Common::StructuredLogger::Log(
        Helianthus::Common::StructuredLogLevel::INFO,
        "MQ",
        "DLQMonitorTest started",
        Helianthus::Common::LogFields{},
        __FILE__,
        __LINE__,
        SPDLOG_FUNCTION
    );
    
    // 设置MQ分类的最小级别
    MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus DLQ监控测试 ===");
    
    // 创建消息队列实例
    auto queue = std::make_unique<MessageQueue>();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建消息队列实例");
    
    // 初始化消息队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化消息队列...");
    auto initResult = queue->Initialize();
    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "消息队列初始化失败: {}", static_cast<int>(initResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息队列初始化成功");

    // 配置指标输出间隔为2秒
    queue->SetGlobalConfig("metrics.interval.ms", "2000");
    // 配置窗口时长与时延样本容量
    queue->SetGlobalConfig("metrics.window.ms", "60000");
    queue->SetGlobalConfig("metrics.latency.capacity", "1024");
    
    // 创建队列配置
    QueueConfig config;
    config.Name = "test_dlq_monitor_queue";
    config.Type = QueueType::STANDARD;
    config.Persistence = PersistenceMode::MEMORY_ONLY;  // 使用内存模式避免文件持久化问题
    config.MaxSize = 1000;
    config.MaxSizeBytes = 1024 * 1024;  // 1MB
    config.MessageTtlMs = 30000;  // 30秒
    config.EnableDeadLetter = true;
    config.EnablePriority = false;
    config.EnableBatching = false;
    config.MaxRetries = 3;
    config.RetryDelayMs = 1000;
    config.EnableRetryBackoff = true;
    config.RetryBackoffMultiplier = 2.0;
    config.MaxRetryDelayMs = 10000;
    config.DeadLetterTtlMs = 86400000;  // 24小时
    
    // 创建队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列: {}", config.Name);
    auto createResult = queue->CreateQueue(config);
    if (createResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "创建队列失败: {}", static_cast<int>(createResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列成功: {}", config.Name);

    // 获取一次队列指标进行验证
    {
        QueueMetrics m;
        if (queue->GetQueueMetrics(config.Name, m) == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
                  "初始指标: queue={}, pending={}, total={}, processed={}, dlq={}, retried={}, enq_rate={:.2f}/s, deq_rate={:.2f}/s, p50={:.2f}ms, p95={:.2f}ms",
                  m.QueueName, m.PendingMessages, m.TotalMessages, m.ProcessedMessages,
                  m.DeadLetterMessages, m.RetriedMessages, m.EnqueueRate, m.DequeueRate,
                  m.P50LatencyMs, m.P95LatencyMs);
        }
    }
    
    // 设置DLQ告警处理器
    queue->SetDeadLetterAlertHandler(OnDeadLetterAlert);
    queue->SetDeadLetterStatsHandler(OnDeadLetterStats);
    
    // 设置DLQ告警配置
    DeadLetterAlertConfig alertConfig;
    alertConfig.MaxDeadLetterMessages = 5;  // 最大5条死信消息
    alertConfig.MaxDeadLetterRate = 0.1;    // 最大10%死信率
    alertConfig.AlertCheckIntervalMs = 5000; // 5秒检查一次
    alertConfig.EnableDeadLetterRateAlert = true;
    alertConfig.EnableDeadLetterCountAlert = true;
    alertConfig.EnableDeadLetterTrendAlert = true;
    
    auto alertResult = queue->SetDeadLetterAlertConfig(config.Name, alertConfig);
    if (alertResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "设置DLQ告警配置失败: {}", static_cast<int>(alertResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "DLQ告警配置设置成功");
    
    // 测试1：发送正常消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试1：发送正常消息 ===");
    
    for (int i = 1; i <= 10; ++i)
    {
        auto message = std::make_shared<Message>(MessageType::TEXT, "正常消息 " + std::to_string(i));
        message->Header.Priority = MessagePriority::NORMAL;
        message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        
        auto sendResult = queue->SendMessage(config.Name, message);
        if (sendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送正常消息成功 id={}", message->Header.Id);
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送正常消息失败: {}", static_cast<int>(sendResult));
        }
    }
    
    // 检查DLQ统计
    DeadLetterQueueStats stats;
    auto statsResult = queue->GetDeadLetterQueueStats(config.Name, stats);
    if (statsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "DLQ统计: totalDLQ={}, currentDLQ={}, expired={}, rejected={}, rate={:.2f}%", 
              stats.TotalDeadLetterMessages, stats.CurrentDeadLetterMessages,
              stats.ExpiredMessages, stats.RejectedMessages, stats.DeadLetterRate * 100.0);
    }
    
    // 测试2：发送过期消息（触发死信）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试2：发送过期消息 ===");
    
    for (int i = 1; i <= 8; ++i)
    {
        auto expiredMessage = std::make_shared<Message>(MessageType::TEXT, "过期消息 " + std::to_string(i));
        expiredMessage->Header.Priority = MessagePriority::NORMAL;
        expiredMessage->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        expiredMessage->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + 1000;  // 1秒后过期
        
        auto sendResult = queue->SendMessage(config.Name, expiredMessage);
        if (sendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "发送过期消息成功 id={}", expiredMessage->Header.Id);
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "发送过期消息失败: {}", static_cast<int>(sendResult));
        }
    }
    
    // 等待消息过期
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待消息过期...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 尝试接收消息，这会触发过期检查
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "尝试接收消息以触发过期检查...");
    for (int i = 0; i < 10; ++i)
    {
        MessagePtr receivedMessage;
        auto receiveResult = queue->ReceiveMessage(config.Name, receivedMessage, 100);
        if (receiveResult == QueueResult::TIMEOUT)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收超时，可能消息已过期");
        }
        else if (receiveResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息: {}", receivedMessage->Header.Id);
        }
    }
    
    // 检查DLQ统计
    statsResult = queue->GetDeadLetterQueueStats(config.Name, stats);
    if (statsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "DLQ统计: totalDLQ={}, currentDLQ={}, expired={}, rejected={}, rate={:.2f}%", 
              stats.TotalDeadLetterMessages, stats.CurrentDeadLetterMessages,
              stats.ExpiredMessages, stats.RejectedMessages, stats.DeadLetterRate * 100.0);
    }
    
    // 尝试接收更多消息，包括过期消息
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "尝试接收更多消息...");
    for (int i = 0; i < 10; ++i)
    {
        MessagePtr receivedMessage;
        auto receiveResult = queue->ReceiveMessage(config.Name, receivedMessage, 100);
        if (receiveResult == QueueResult::TIMEOUT)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收超时");
        }
        else if (receiveResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "接收到消息: {}", receivedMessage->Header.Id);
        }
    }
    
    // 再次检查DLQ统计
    statsResult = queue->GetDeadLetterQueueStats(config.Name, stats);
    if (statsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "最终DLQ统计: totalDLQ={}, currentDLQ={}, expired={}, rejected={}, rate={:.2f}%", 
              stats.TotalDeadLetterMessages, stats.CurrentDeadLetterMessages,
              stats.ExpiredMessages, stats.RejectedMessages, stats.DeadLetterRate * 100.0);
    }
    
    // 测试3：检查活跃告警
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试3：检查活跃告警 ===");
    
    std::vector<DeadLetterAlert> alerts;
    auto alertsResult = queue->GetActiveDeadLetterAlerts(config.Name, alerts);
    if (alertsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "活跃告警数量: {}", alerts.size());
        for (const auto& alert : alerts)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, 
                  "告警: type={}, message={}, currentValue={}, thresholdValue={}", 
                  static_cast<int>(alert.Type), alert.AlertMessage, 
                  alert.CurrentValue, alert.ThresholdValue);
        }
    }
    
    // 测试4：清除告警
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试4：清除告警 ===");
    
    auto clearResult = queue->ClearAllDeadLetterAlerts(config.Name);
    if (clearResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "所有告警已清除");
    }
    
    // 再次检查告警
    alerts.clear();
    alertsResult = queue->GetActiveDeadLetterAlerts(config.Name, alerts);
    if (alertsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "清除后活跃告警数量: {}", alerts.size());
    }
    
    // 测试5：获取所有DLQ统计
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试5：获取所有DLQ统计 ===");
    
    std::vector<DeadLetterQueueStats> allStats;
    auto allStatsResult = queue->GetAllDeadLetterQueueStats(allStats);
    if (allStatsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "所有DLQ统计数量: {}", allStats.size());
        for (const auto& stat : allStats)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
                  "队列: {}, 总死信: {}, 当前死信: {}, 死信率: {:.2f}%", 
                  stat.QueueName, stat.TotalDeadLetterMessages, 
                  stat.CurrentDeadLetterMessages, stat.DeadLetterRate * 100.0);
        }
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== DLQ监控测试完成 ===");

    // 获取最终一次队列指标
    {
        QueueMetrics m;
        if (queue->GetQueueMetrics(config.Name, m) == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
                  "最终指标: queue={}, pending={}, total={}, processed={}, dlq={}, retried={}, enq_rate={:.2f}/s, deq_rate={:.2f}/s, p50={:.2f}ms, p95={:.2f}ms",
                  m.QueueName, m.PendingMessages, m.TotalMessages, m.ProcessedMessages,
                  m.DeadLetterMessages, m.RetriedMessages, m.EnqueueRate, m.DequeueRate,
                  m.P50LatencyMs, m.P95LatencyMs);
        }
    }
    // 结束前刷新结构化日志
    Helianthus::Common::StructuredLogger::Log(
        Helianthus::Common::StructuredLogLevel::INFO,
        "MQ",
        "DLQMonitorTest finished",
        Helianthus::Common::LogFields{},
        __FILE__,
        __LINE__,
        SPDLOG_FUNCTION
    );
    // 清理线程本地上下文字段
    Helianthus::Common::StructuredLogger::ClearAllThreadFields();
    
    return 0;
}
