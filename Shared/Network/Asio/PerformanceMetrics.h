#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory> // Added for std::unique_ptr
#include <deque>

namespace Helianthus::Network::Asio
{
    // 延迟分位数计算器
    class LatencyHistogram
    {
    public:
        LatencyHistogram(size_t MaxSamples = 10000);
        
        void AddSample(uint64_t LatencyNs);
        double GetPercentile(double Percentile) const; // 0.0-1.0
        void Reset();
        
        // 获取常用分位数
        double GetP50() const { return GetPercentile(0.50); }
        double GetP95() const { return GetPercentile(0.95); }
        double GetP99() const { return GetPercentile(0.99); }
        double GetP999() const { return GetPercentile(0.999); }
        
        size_t GetSampleCount() const { return Samples.size(); }
        
    private:
        std::deque<uint64_t> Samples;
        size_t MaxSamples;
        mutable std::mutex SamplesMutex;
    };

    // 错误分类统计
    struct ErrorStats
    {
        std::atomic<uint64_t> NetworkErrors{0};
        std::atomic<uint64_t> TimeoutErrors{0};
        std::atomic<uint64_t> ProtocolErrors{0};
        std::atomic<uint64_t> AuthenticationErrors{0};
        std::atomic<uint64_t> AuthorizationErrors{0};
        std::atomic<uint64_t> ResourceErrors{0};
        std::atomic<uint64_t> SystemErrors{0};
        std::atomic<uint64_t> UnknownErrors{0};
        
        void Reset() {
            NetworkErrors = 0;
            TimeoutErrors = 0;
            ProtocolErrors = 0;
            AuthenticationErrors = 0;
            AuthorizationErrors = 0;
            ResourceErrors = 0;
            SystemErrors = 0;
            UnknownErrors = 0;
        }
        
        uint64_t GetTotalErrors() const {
            return NetworkErrors.load() + TimeoutErrors.load() + ProtocolErrors.load() +
                   AuthenticationErrors.load() + AuthorizationErrors.load() + ResourceErrors.load() +
                   SystemErrors.load() + UnknownErrors.load();
        }
    };

    // 连接池统计
    struct ConnectionPoolStats
    {
        std::atomic<uint32_t> TotalPoolSize{0};
        std::atomic<uint32_t> ActiveConnections{0};
        std::atomic<uint32_t> IdleConnections{0};
        std::atomic<uint32_t> MaxConnections{0};
        std::atomic<uint64_t> ConnectionWaitTimeMs{0};
        std::atomic<uint64_t> ConnectionWaitCount{0};
        std::atomic<uint64_t> PoolExhaustionCount{0};
        
        ConnectionPoolStats() = default;
        
        // 拷贝构造函数
        ConnectionPoolStats(const ConnectionPoolStats& Other) {
            TotalPoolSize = Other.TotalPoolSize.load();
            ActiveConnections = Other.ActiveConnections.load();
            IdleConnections = Other.IdleConnections.load();
            MaxConnections = Other.MaxConnections.load();
            ConnectionWaitTimeMs = Other.ConnectionWaitTimeMs.load();
            ConnectionWaitCount = Other.ConnectionWaitCount.load();
            PoolExhaustionCount = Other.PoolExhaustionCount.load();
        }
        
        // 赋值操作符
        ConnectionPoolStats& operator=(const ConnectionPoolStats& Other) {
            if (this != &Other) {
                TotalPoolSize = Other.TotalPoolSize.load();
                ActiveConnections = Other.ActiveConnections.load();
                IdleConnections = Other.IdleConnections.load();
                MaxConnections = Other.MaxConnections.load();
                ConnectionWaitTimeMs = Other.ConnectionWaitTimeMs.load();
                ConnectionWaitCount = Other.ConnectionWaitCount.load();
                PoolExhaustionCount = Other.PoolExhaustionCount.load();
            }
            return *this;
        }
        
        void Reset() {
            TotalPoolSize = 0;
            ActiveConnections = 0;
            IdleConnections = 0;
            MaxConnections = 0;
            ConnectionWaitTimeMs = 0;
            ConnectionWaitCount = 0;
            PoolExhaustionCount = 0;
        }
        
        double GetAverageWaitTimeMs() const {
            uint64_t Count = ConnectionWaitCount.load();
            if (Count == 0) return 0.0;
            return static_cast<double>(ConnectionWaitTimeMs.load()) / Count;
        }
        
        double GetPoolUtilization() const {
            uint32_t Max = MaxConnections.load();
            if (Max == 0) return 0.0;
            return static_cast<double>(ActiveConnections.load()) / Max;
        }
    };

    // 资源使用统计
    struct ResourceUsageStats
    {
        std::atomic<uint64_t> MemoryUsageBytes{0};
        std::atomic<uint64_t> PeakMemoryUsageBytes{0};
        std::atomic<uint32_t> ThreadCount{0};
        std::atomic<uint64_t> CpuUsagePercent{0};
        std::atomic<uint64_t> FileDescriptorCount{0};
        std::atomic<uint64_t> BufferPoolUsage{0};
        std::atomic<uint64_t> BufferPoolCapacity{0};
        
        ResourceUsageStats() = default;
        
        // 拷贝构造函数
        ResourceUsageStats(const ResourceUsageStats& Other) {
            MemoryUsageBytes = Other.MemoryUsageBytes.load();
            PeakMemoryUsageBytes = Other.PeakMemoryUsageBytes.load();
            ThreadCount = Other.ThreadCount.load();
            CpuUsagePercent = Other.CpuUsagePercent.load();
            FileDescriptorCount = Other.FileDescriptorCount.load();
            BufferPoolUsage = Other.BufferPoolUsage.load();
            BufferPoolCapacity = Other.BufferPoolCapacity.load();
        }
        
        // 赋值操作符
        ResourceUsageStats& operator=(const ResourceUsageStats& Other) {
            if (this != &Other) {
                MemoryUsageBytes = Other.MemoryUsageBytes.load();
                PeakMemoryUsageBytes = Other.PeakMemoryUsageBytes.load();
                ThreadCount = Other.ThreadCount.load();
                CpuUsagePercent = Other.CpuUsagePercent.load();
                FileDescriptorCount = Other.FileDescriptorCount.load();
                BufferPoolUsage = Other.BufferPoolUsage.load();
                BufferPoolCapacity = Other.BufferPoolCapacity.load();
            }
            return *this;
        }
        
        void Reset() {
            MemoryUsageBytes = 0;
            PeakMemoryUsageBytes = 0;
            ThreadCount = 0;
            CpuUsagePercent = 0;
            FileDescriptorCount = 0;
            BufferPoolUsage = 0;
            BufferPoolCapacity = 0;
        }
        
        double GetBufferPoolUtilization() const {
            uint64_t Capacity = BufferPoolCapacity.load();
            if (Capacity == 0) return 0.0;
            return static_cast<double>(BufferPoolUsage.load()) / Capacity;
        }
    };

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
        
        // 延迟分位数统计
        mutable std::unique_ptr<LatencyHistogram> LatencyHistogramPtr;
        mutable std::mutex HistogramMutex;
        
        // 错误分类统计
        ErrorStats ErrorStatistics;
        
        // 吞吐量统计
        std::atomic<uint64_t> TotalBytesProcessed{0};
        std::atomic<uint64_t> TotalMessagesProcessed{0};
        
        // 时间窗口统计
        std::chrono::steady_clock::time_point LastResetTime;
        
        PerformanceMetrics() = default;
        ~PerformanceMetrics() = default;
        
        // 禁用拷贝构造和赋值
        PerformanceMetrics(const PerformanceMetrics&) = delete;
        PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
        
        // 启用移动构造和赋值
        PerformanceMetrics(PerformanceMetrics&&) = default;
        PerformanceMetrics& operator=(PerformanceMetrics&&) = default;
        
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
        
        // 获取延迟分位数（毫秒）
        double GetLatencyPercentileMs(double Percentile) const {
            std::lock_guard<std::mutex> Lock(HistogramMutex);
            if (!LatencyHistogramPtr) return 0.0;
            return LatencyHistogramPtr->GetPercentile(Percentile) / 1'000'000.0;
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
        
        // 添加延迟样本
        void AddLatencySample(uint64_t LatencyNs) {
            std::lock_guard<std::mutex> Lock(HistogramMutex);
            if (!LatencyHistogramPtr) {
                LatencyHistogramPtr = std::make_unique<LatencyHistogram>();
            }
            LatencyHistogramPtr->AddSample(LatencyNs);
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
            
            ErrorStatistics.Reset();
            
            std::lock_guard<std::mutex> Lock(HistogramMutex);
            if (LatencyHistogramPtr) {
                LatencyHistogramPtr->Reset();
            }
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
        
        // 连接池统计
        ConnectionPoolStats PoolStats;
        
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
        ResourceUsageStats ResourceStats;
        
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
        
        // 错误分类更新
        void UpdateErrorStats(const std::string& ConnectionId, const std::string& ErrorType);
        void UpdateOperationErrorStats(const std::string& OperationId, const std::string& ErrorType);
        
        // 连接池管理
        void UpdateConnectionPoolStats(const std::string& ConnectionId, const ConnectionPoolStats& Stats);
        
        // 系统统计
        void UpdateSystemMetrics(const SystemMetrics& Metrics);
        void UpdateResourceUsage(const ResourceUsageStats& Stats);
        
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
