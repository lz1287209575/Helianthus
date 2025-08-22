#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory> // Added for std::unique_ptr

namespace Helianthus::Network::Asio
{
    // 基础性能指标
    struct PerformanceMetrics
    {
        // 计数器
        std::atomic<uint64_t> TotalOperations{0};
        std::atomic<uint64_t> SuccessfulOperations{0};
        std::atomic<uint64_t> FailedOperations{0};
        std::atomic<uint64_t> TimeoutOperations{0};
        std::atomic<uint64_t> CancelledOperations{0};
        
        // 延迟统计
        std::atomic<uint64_t> TotalLatencyNs{0};
        std::atomic<uint64_t> MinLatencyNs{UINT64_MAX};
        std::atomic<uint64_t> MaxLatencyNs{0};
        std::atomic<uint64_t> LatencyCount{0};
        
        // 吞吐量统计
        std::atomic<uint64_t> TotalBytesProcessed{0};
        std::atomic<uint64_t> TotalMessagesProcessed{0};
        
        // 时间窗口统计
        std::chrono::steady_clock::time_point LastResetTime;
        
        // 计算平均延迟（纳秒）
        double GetAverageLatencyNs() const {
            uint64_t Count = LatencyCount.load();
            if (Count == 0) return 0.0;
            return static_cast<double>(TotalLatencyNs.load()) / Count;
        }
        
        // 计算平均延迟（毫秒）
        double GetAverageLatencyMs() const {
            return GetAverageLatencyNs() / 1'000'000.0;
        }
        
        // 计算成功率
        double GetSuccessRate() const {
            uint64_t Total = TotalOperations.load();
            if (Total == 0) return 0.0;
            return static_cast<double>(SuccessfulOperations.load()) / Total;
        }
        
        // 计算吞吐量（操作/秒）
        double GetThroughputOpsPerSec() const {
            auto Now = std::chrono::steady_clock::now();
            auto Duration = std::chrono::duration_cast<std::chrono::seconds>(Now - LastResetTime);
            if (Duration.count() == 0) return 0.0;
            return static_cast<double>(TotalOperations.load()) / Duration.count();
        }
        
        // 重置统计
        void Reset() {
            TotalOperations = 0;
            SuccessfulOperations = 0;
            FailedOperations = 0;
            TimeoutOperations = 0;
            CancelledOperations = 0;
            TotalLatencyNs = 0;
            MinLatencyNs = UINT64_MAX;
            MaxLatencyNs = 0;
            LatencyCount = 0;
            TotalBytesProcessed = 0;
            TotalMessagesProcessed = 0;
            LastResetTime = std::chrono::steady_clock::now();
        }
    };

    // 连接级别统计
    struct ConnectionMetrics : public PerformanceMetrics
    {
        std::string ConnectionId;
        std::string RemoteAddress;
        std::chrono::steady_clock::time_point ConnectionTime;
        
        // 连接特定统计
        std::atomic<uint64_t> ReconnectCount{0};
        std::atomic<uint64_t> ConnectionErrors{0};
        std::atomic<uint64_t> ProtocolErrors{0};
        
        // 计算连接持续时间（秒）
        double GetConnectionDurationSec() const {
            auto Now = std::chrono::steady_clock::now();
            auto Duration = std::chrono::duration_cast<std::chrono::seconds>(Now - ConnectionTime);
            return static_cast<double>(Duration.count());
        }
    };

    // 操作级别统计
    struct OperationMetrics : public PerformanceMetrics
    {
        std::string OperationType; // "send", "receive", "connect", etc.
        std::string Protocol;      // "tcp", "udp", etc.
        
        // 操作特定统计
        std::atomic<uint64_t> PartialOperations{0};
        std::atomic<uint64_t> RetryCount{0};
        std::atomic<uint64_t> BufferOverflows{0};
    };

    // 系统级别统计
    struct SystemMetrics
    {
        // 连接统计
        std::atomic<uint32_t> ActiveConnections{0};
        std::atomic<uint32_t> TotalConnections{0};
        std::atomic<uint32_t> FailedConnections{0};
        
        // 资源使用统计
        std::atomic<uint64_t> MemoryUsageBytes{0};
        std::atomic<uint32_t> ThreadCount{0};
        std::atomic<uint64_t> CpuUsagePercent{0};
        
        // 事件循环统计
        std::atomic<uint64_t> EventLoopIterations{0};
        std::atomic<uint64_t> EventsProcessed{0};
        std::atomic<uint64_t> IdleTimeMs{0};
        
        // 批处理统计
        std::atomic<uint64_t> BatchProcessingCount{0};
        std::atomic<uint64_t> AverageBatchSize{0};
        std::atomic<uint64_t> MaxBatchSize{0};
        
        // 复制构造函数
        SystemMetrics() = default;
        SystemMetrics(const SystemMetrics& Other) {
            ActiveConnections = Other.ActiveConnections.load();
            TotalConnections = Other.TotalConnections.load();
            FailedConnections = Other.FailedConnections.load();
            MemoryUsageBytes = Other.MemoryUsageBytes.load();
            ThreadCount = Other.ThreadCount.load();
            CpuUsagePercent = Other.CpuUsagePercent.load();
            EventLoopIterations = Other.EventLoopIterations.load();
            EventsProcessed = Other.EventsProcessed.load();
            IdleTimeMs = Other.IdleTimeMs.load();
            BatchProcessingCount = Other.BatchProcessingCount.load();
            AverageBatchSize = Other.AverageBatchSize.load();
            MaxBatchSize = Other.MaxBatchSize.load();
        }
        
        // 赋值操作符
        SystemMetrics& operator=(const SystemMetrics& Other) {
            if (this != &Other) {
                ActiveConnections = Other.ActiveConnections.load();
                TotalConnections = Other.TotalConnections.load();
                FailedConnections = Other.FailedConnections.load();
                MemoryUsageBytes = Other.MemoryUsageBytes.load();
                ThreadCount = Other.ThreadCount.load();
                CpuUsagePercent = Other.CpuUsagePercent.load();
                EventLoopIterations = Other.EventLoopIterations.load();
                EventsProcessed = Other.EventsProcessed.load();
                IdleTimeMs = Other.IdleTimeMs.load();
                BatchProcessingCount = Other.BatchProcessingCount.load();
                AverageBatchSize = Other.AverageBatchSize.load();
                MaxBatchSize = Other.MaxBatchSize.load();
            }
            return *this;
        }
    };

    // Prometheus 格式导出
    class PrometheusExporter
    {
    public:
        // 导出连接指标
        static std::string ExportConnectionMetrics(const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& Metrics);
        
        // 导出操作指标
        static std::string ExportOperationMetrics(const std::map<std::string, std::unique_ptr<OperationMetrics>>& Metrics);
        
        // 导出系统指标
        static std::string ExportSystemMetrics(const SystemMetrics& Metrics);
        
        // 导出所有指标
        static std::string ExportAllMetrics(
            const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& ConnectionMetrics,
            const std::map<std::string, std::unique_ptr<OperationMetrics>>& OperationMetrics,
            const SystemMetrics& SystemMetrics);

    private:
        static std::string FormatMetric(const std::string& Name, double Value, 
                                       const std::map<std::string, std::string>& Labels = {});
        static std::string FormatHistogram(const std::string& Name, 
                                          const std::vector<double>& Buckets,
                                          const std::map<std::string, std::string>& Labels = {});
    };

    // 性能监控管理器
    class PerformanceMonitor
    {
    public:
        static PerformanceMonitor& Instance();
        
        // 连接管理
        void RegisterConnection(const std::string& ConnectionId, const std::string& RemoteAddress);
        void UnregisterConnection(const std::string& ConnectionId);
        void UpdateConnectionMetrics(const std::string& ConnectionId, 
                                   bool Success, uint64_t LatencyNs, 
                                   uint64_t BytesProcessed = 0);
        
        // 操作管理
        void RegisterOperation(const std::string& OperationId, 
                              const std::string& OperationType, 
                              const std::string& Protocol);
        void UpdateOperationMetrics(const std::string& OperationId, 
                                  bool Success, uint64_t LatencyNs,
                                  uint64_t BytesProcessed = 0);
        
        // 系统统计
        void UpdateSystemMetrics(const SystemMetrics& Metrics);
        
        // 获取统计 - 返回引用避免复制
        const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& GetConnectionMetrics() const;
        const std::map<std::string, std::unique_ptr<OperationMetrics>>& GetOperationMetrics() const;
        SystemMetrics GetSystemMetrics() const;
        
        // 导出 Prometheus 格式
        std::string ExportPrometheusMetrics() const;
        
        // 重置统计
        void ResetAllMetrics();
        void ResetConnectionMetrics(const std::string& ConnectionId = "");
        void ResetOperationMetrics(const std::string& OperationId = "");

    private:
        PerformanceMonitor() = default;
        
        mutable std::mutex ConnectionMetricsMutex;
        std::map<std::string, std::unique_ptr<ConnectionMetrics>> ConnectionMetricsMap;
        
        mutable std::mutex OperationMetricsMutex;
        std::map<std::string, std::unique_ptr<OperationMetrics>> OperationMetricsMap;
        
        mutable std::mutex SystemMetricsMutex;
        SystemMetrics CurrentSystemMetrics;
    };

    // 便捷宏
    #define PERFORMANCE_MONITOR() Helianthus::Network::Asio::PerformanceMonitor::Instance()
    
    // 自动计时器
    class ScopedTimer
    {
    public:
        ScopedTimer(const std::string& OperationId, const std::string& OperationType, const std::string& Protocol = "")
            : OperationId_(OperationId)
            , StartTime_(std::chrono::steady_clock::now())
        {
            PERFORMANCE_MONITOR().RegisterOperation(OperationId, OperationType, Protocol);
        }
        
        ~ScopedTimer()
        {
            auto EndTime = std::chrono::steady_clock::now();
            auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime_);
            PERFORMANCE_MONITOR().UpdateOperationMetrics(OperationId_, true, Duration.count());
        }
        
        void SetSuccess(bool Success) { Success_ = Success; }
        void SetBytesProcessed(uint64_t Bytes) { BytesProcessed_ = Bytes; }
        
    private:
        std::string OperationId_;
        std::chrono::steady_clock::time_point StartTime_;
        bool Success_ = true;
        uint64_t BytesProcessed_ = 0;
    };
    
    #define PERFORMANCE_TIMER(OperationId, OperationType, Protocol) \
        Helianthus::Network::Asio::ScopedTimer timer(OperationId, OperationType, Protocol)
}
