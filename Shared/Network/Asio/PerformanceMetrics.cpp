#include "Shared/Network/Asio/PerformanceMetrics.h"
#include "Common/LogCategory.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

// 创建本地日志类别实例
H_DEFINE_LOG_CATEGORY(Perf, Helianthus::Common::LogVerbosity::Log);

namespace Helianthus::Network::Asio
{
    // LatencyHistogram 实现
    LatencyHistogram::LatencyHistogram(size_t MaxSamples)
        : MaxSamples(MaxSamples)
    {
    }

    void LatencyHistogram::AddSample(uint64_t LatencyNs)
    {
        std::lock_guard<std::mutex> Lock(SamplesMutex);
        Samples.push_back(LatencyNs);

        // 保持样本数量在限制范围内
        if (Samples.size() > MaxSamples) {
            Samples.pop_front();
        }
    }

    double LatencyHistogram::GetPercentile(double Percentile) const
    {
        std::lock_guard<std::mutex> Lock(SamplesMutex);
        if (Samples.empty()) return 0.0;
        
        if (Percentile <= 0.0) return static_cast<double>(Samples.front());
        if (Percentile >= 1.0) return static_cast<double>(Samples.back());
        
        // 创建排序副本
        std::vector<uint64_t> SortedSamples(Samples.begin(), Samples.end());
        std::sort(SortedSamples.begin(), SortedSamples.end());
        
        double Index = Percentile * (SortedSamples.size() - 1);
        size_t LowerIndex = static_cast<size_t>(Index);
        size_t UpperIndex = LowerIndex + 1;
        
        if (UpperIndex >= SortedSamples.size()) {
            return static_cast<double>(SortedSamples[LowerIndex]);
        }
        
        double Fraction = Index - LowerIndex;
        return static_cast<double>(SortedSamples[LowerIndex]) * (1.0 - Fraction) +
               static_cast<double>(SortedSamples[UpperIndex]) * Fraction;
    }

    void LatencyHistogram::Reset()
    {
        std::lock_guard<std::mutex> Lock(SamplesMutex);
        Samples.clear();
    }

    // PerformanceMonitor 单例实现
    PerformanceMonitor& PerformanceMonitor::Instance()
    {
        static PerformanceMonitor instance;
        return instance;
    }

    // 连接管理
    void PerformanceMonitor::RegisterConnection(const std::string& ConnectionId, const std::string& RemoteAddress)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
        auto Metrics = std::make_unique<ConnectionMetrics>();
        Metrics->ConnectionId = ConnectionId;
        Metrics->RemoteAddress = RemoteAddress;
        Metrics->ConnectionTime = std::chrono::steady_clock::now();
        Metrics->Reset();
        Metrics->LastResetTime = std::chrono::steady_clock::now(); // 在Reset()之后设置LastResetTime
        ConnectionMetricsMap[ConnectionId] = std::move(Metrics);
        H_LOG(Perf, Helianthus::Common::LogVerbosity::Verbose, "RegisterConnection id={} addr={}", ConnectionId, RemoteAddress);
        
        // 更新系统指标
        {
            std::lock_guard<std::mutex> SysLock(SystemMetricsMutex);
            CurrentSystemMetrics.ActiveConnections++;
            CurrentSystemMetrics.TotalConnections++;
        }
    }

    void PerformanceMonitor::UnregisterConnection(const std::string& ConnectionId)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
        auto It = ConnectionMetricsMap.find(ConnectionId);
        if (It != ConnectionMetricsMap.end()) {
            ConnectionMetricsMap.erase(ConnectionId);
            
            // 更新系统指标
            {
                std::lock_guard<std::mutex> SysLock(SystemMetricsMutex);
                if (CurrentSystemMetrics.ActiveConnections.load() > 0) {
                    CurrentSystemMetrics.ActiveConnections--;
                }
            }
        }
    }

    void PerformanceMonitor::UpdateConnectionMetrics(const std::string& ConnectionId, 
                                                   bool Success, uint64_t LatencyNs, 
                                                   uint64_t BytesProcessed)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
        auto It = ConnectionMetricsMap.find(ConnectionId);
        if (It == ConnectionMetricsMap.end()) { H_LOG(Perf, Helianthus::Common::LogVerbosity::Warning, "UpdateConnectionMetrics unknown id={}", ConnectionId); return; }

        ConnectionMetrics& Metrics = *It->second;
        Metrics.TotalOperations++;
        
        if (Success) {
            Metrics.SuccessfulOperations++;
        } else {
            Metrics.FailedOperations++;
        }

        // 更新延迟统计
        Metrics.TotalLatencyNs += LatencyNs;
        Metrics.LatencyCount++;
        Metrics.AddLatencySample(LatencyNs);
        
        uint64_t CurrentMin = Metrics.MinLatencyNs.load();
        while (LatencyNs < CurrentMin && 
               !Metrics.MinLatencyNs.compare_exchange_weak(CurrentMin, LatencyNs)) {
            // 重试直到成功更新最小值
        }
        
        uint64_t CurrentMax = Metrics.MaxLatencyNs.load();
        while (LatencyNs > CurrentMax && 
               !Metrics.MaxLatencyNs.compare_exchange_weak(CurrentMax, LatencyNs)) {
            // 重试直到成功更新最大值
        }

        // 更新吞吐量统计
        Metrics.TotalBytesProcessed += BytesProcessed;
        Metrics.TotalMessagesProcessed++;
    }

    // 操作管理
    void PerformanceMonitor::RegisterOperation(const std::string& OperationId, 
                                              const std::string& OperationType, 
                                              const std::string& Protocol)
    {
        std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
        auto Metrics = std::make_unique<OperationMetrics>();
        Metrics->OperationType = OperationType;
        Metrics->Protocol = Protocol;
        Metrics->Reset();
        OperationMetricsMap[OperationId] = std::move(Metrics);
        H_LOG(Perf, Helianthus::Common::LogVerbosity::Verbose, "RegisterOperation id={} type={} proto={}", OperationId, OperationType, Protocol);
    }

    void PerformanceMonitor::UpdateOperationMetrics(const std::string& OperationId, 
                                                  bool Success, uint64_t LatencyNs,
                                                  uint64_t BytesProcessed)
    {
        std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
        auto It = OperationMetricsMap.find(OperationId);
        if (It == OperationMetricsMap.end()) { H_LOG(Perf, Helianthus::Common::LogVerbosity::Warning, "UpdateOperationMetrics unknown id={}", OperationId); return; }

        OperationMetrics& Metrics = *It->second;
        Metrics.TotalOperations++;
        
        if (Success) {
            Metrics.SuccessfulOperations++;
        } else {
            Metrics.FailedOperations++;
        }

        // 更新延迟统计
        Metrics.TotalLatencyNs += LatencyNs;
        Metrics.LatencyCount++;
        Metrics.AddLatencySample(LatencyNs);
        
        uint64_t CurrentMin = Metrics.MinLatencyNs.load();
        while (LatencyNs < CurrentMin && 
               !Metrics.MinLatencyNs.compare_exchange_weak(CurrentMin, LatencyNs)) {
            // 重试直到成功更新最小值
        }
        
        uint64_t CurrentMax = Metrics.MaxLatencyNs.load();
        while (LatencyNs > CurrentMax && 
               !Metrics.MaxLatencyNs.compare_exchange_weak(CurrentMax, LatencyNs)) {
            // 重试直到成功更新最大值
        }

        // 更新吞吐量统计
        Metrics.TotalBytesProcessed += BytesProcessed;
        Metrics.TotalMessagesProcessed++;
    }

    // 错误分类更新
    void PerformanceMonitor::UpdateErrorStats(const std::string& ConnectionId, const std::string& ErrorType)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
        auto It = ConnectionMetricsMap.find(ConnectionId);
        if (It == ConnectionMetricsMap.end()) return;

        ConnectionMetrics& Metrics = *It->second;
        
        if (ErrorType == "network") {
            Metrics.ErrorStatistics.NetworkErrors++;
        } else if (ErrorType == "timeout") {
            Metrics.ErrorStatistics.TimeoutErrors++;
        } else if (ErrorType == "protocol") {
            Metrics.ErrorStatistics.ProtocolErrors++;
        } else if (ErrorType == "authentication") {
            Metrics.ErrorStatistics.AuthenticationErrors++;
        } else if (ErrorType == "authorization") {
            Metrics.ErrorStatistics.AuthorizationErrors++;
        } else if (ErrorType == "resource") {
            Metrics.ErrorStatistics.ResourceErrors++;
        } else if (ErrorType == "system") {
            Metrics.ErrorStatistics.SystemErrors++;
        } else {
            Metrics.ErrorStatistics.UnknownErrors++;
        }
    }

    void PerformanceMonitor::UpdateOperationErrorStats(const std::string& OperationId, const std::string& ErrorType)
    {
        std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
        auto It = OperationMetricsMap.find(OperationId);
        if (It == OperationMetricsMap.end()) return;

        OperationMetrics& Metrics = *It->second;
        
        if (ErrorType == "network") {
            Metrics.ErrorStatistics.NetworkErrors++;
        } else if (ErrorType == "timeout") {
            Metrics.ErrorStatistics.TimeoutErrors++;
        } else if (ErrorType == "protocol") {
            Metrics.ErrorStatistics.ProtocolErrors++;
        } else if (ErrorType == "authentication") {
            Metrics.ErrorStatistics.AuthenticationErrors++;
        } else if (ErrorType == "authorization") {
            Metrics.ErrorStatistics.AuthorizationErrors++;
        } else if (ErrorType == "resource") {
            Metrics.ErrorStatistics.ResourceErrors++;
        } else if (ErrorType == "system") {
            Metrics.ErrorStatistics.SystemErrors++;
        } else {
            Metrics.ErrorStatistics.UnknownErrors++;
        }
    }

    // 连接池管理
    void PerformanceMonitor::UpdateConnectionPoolStats(const std::string& ConnectionId, const ConnectionPoolStats& Stats)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
        auto It = ConnectionMetricsMap.find(ConnectionId);
        if (It == ConnectionMetricsMap.end()) return;

        ConnectionMetrics& Metrics = *It->second;
        Metrics.PoolStats = Stats;
    }

    // 系统统计
    void PerformanceMonitor::UpdateSystemMetrics(const SystemMetrics& Metrics)
    {
        std::lock_guard<std::mutex> Lock(SystemMetricsMutex);
        CurrentSystemMetrics = Metrics;
    }

    void PerformanceMonitor::UpdateResourceUsage(const ResourceUsageStats& Stats)
    {
        std::lock_guard<std::mutex> Lock(SystemMetricsMutex);
        CurrentSystemMetrics.ResourceStats = Stats;
    }

    // 获取统计
    const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& PerformanceMonitor::GetConnectionMetrics() const
    {
        return ConnectionMetricsMap;
    }

    const std::map<std::string, std::unique_ptr<OperationMetrics>>& PerformanceMonitor::GetOperationMetrics() const
    {
        return OperationMetricsMap;
    }

    SystemMetrics PerformanceMonitor::GetSystemMetrics() const
    {
        std::lock_guard<std::mutex> Lock(SystemMetricsMutex);
        return CurrentSystemMetrics;
    }

    // Prometheus 导出
    std::string PrometheusExporter::FormatMetric(const std::string& Name, double Value, 
                                               const std::map<std::string, std::string>& Labels)
    {
        std::ostringstream Oss;
        Oss << Name;
        
        if (!Labels.empty()) {
            Oss << "{";
            bool First = true;
            for (const auto& Label : Labels) {
                if (!First) Oss << ",";
                Oss << Label.first << "=\"" << Label.second << "\"";
                First = false;
            }
            Oss << "}";
        }
        
        Oss << " " << std::fixed << std::setprecision(6) << Value << "\n";
        return Oss.str();
    }

    std::string PrometheusExporter::FormatHistogram(const std::string& Name, 
                                                  const std::vector<double>& Buckets,
                                                  const std::map<std::string, std::string>& Labels)
    {
        std::ostringstream Oss;
        
        // 添加桶计数
        for (size_t I = 0; I < Buckets.size(); ++I) {
            auto BucketLabels = Labels;
            BucketLabels["le"] = std::to_string(static_cast<int64_t>(Buckets[I]));
            Oss << FormatMetric(Name + "_bucket", Buckets[I], BucketLabels);
        }
        
        // 添加总和
        Oss << FormatMetric(Name + "_sum", 0.0, Labels);
        
        // 添加计数
        Oss << FormatMetric(Name + "_count", 0.0, Labels);
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportConnectionMetrics(const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& Metrics)
    {
        std::ostringstream Oss;
        
        for (const auto& Pair : Metrics) {
            const auto& ConnectionId = Pair.first;
            const auto& Metrics = Pair.second;
            
            std::map<std::string, std::string> Labels = {
                {"connection_id", ConnectionId},
                {"remote_address", Metrics->RemoteAddress}
            };
            
            // 基础指标
            Oss << FormatMetric("connection_total_operations", Metrics->TotalOperations.load(), Labels);
            Oss << FormatMetric("connection_successful_operations", Metrics->SuccessfulOperations.load(), Labels);
            Oss << FormatMetric("connection_failed_operations", Metrics->FailedOperations.load(), Labels);
            Oss << FormatMetric("connection_success_rate", Metrics->GetSuccessRate(), Labels);
            
            // 延迟指标
            Oss << FormatMetric("connection_avg_latency_ms", Metrics->GetAverageLatencyMs(), Labels);
            Oss << FormatMetric("connection_min_latency_ms", Metrics->MinLatencyNs.load() / 1'000'000.0, Labels);
            Oss << FormatMetric("connection_max_latency_ms", Metrics->MaxLatencyNs.load() / 1'000'000.0, Labels);
            Oss << FormatMetric("connection_p50_latency_ms", Metrics->GetLatencyPercentileMs(0.50), Labels);
            Oss << FormatMetric("connection_p95_latency_ms", Metrics->GetLatencyPercentileMs(0.95), Labels);
            Oss << FormatMetric("connection_p99_latency_ms", Metrics->GetLatencyPercentileMs(0.99), Labels);
            
            // 吞吐量指标
            Oss << FormatMetric("connection_throughput_ops_per_sec", Metrics->GetThroughputOpsPerSec(), Labels);
            Oss << FormatMetric("connection_total_bytes_processed", Metrics->TotalBytesProcessed.load(), Labels);
            Oss << FormatMetric("connection_total_messages_processed", Metrics->TotalMessagesProcessed.load(), Labels);
            
            // 连接特定指标
            Oss << FormatMetric("connection_duration_sec", Metrics->GetConnectionDurationSec(), Labels);
            Oss << FormatMetric("connection_reconnect_count", Metrics->ReconnectCount.load(), Labels);
            
            // 错误分类指标
            Oss << FormatMetric("connection_network_errors", Metrics->ErrorStatistics.NetworkErrors.load(), Labels);
            Oss << FormatMetric("connection_timeout_errors", Metrics->ErrorStatistics.TimeoutErrors.load(), Labels);
            Oss << FormatMetric("connection_protocol_errors", Metrics->ErrorStatistics.ProtocolErrors.load(), Labels);
            Oss << FormatMetric("connection_authentication_errors", Metrics->ErrorStatistics.AuthenticationErrors.load(), Labels);
            Oss << FormatMetric("connection_authorization_errors", Metrics->ErrorStatistics.AuthorizationErrors.load(), Labels);
            Oss << FormatMetric("connection_resource_errors", Metrics->ErrorStatistics.ResourceErrors.load(), Labels);
            Oss << FormatMetric("connection_system_errors", Metrics->ErrorStatistics.SystemErrors.load(), Labels);
            Oss << FormatMetric("connection_unknown_errors", Metrics->ErrorStatistics.UnknownErrors.load(), Labels);
            
            // 连接池指标
            auto PoolLabels = Labels;
            PoolLabels["pool_type"] = "connection";
            Oss << FormatMetric("pool_total_size", Metrics->PoolStats.TotalPoolSize.load(), PoolLabels);
            Oss << FormatMetric("pool_active_connections", Metrics->PoolStats.ActiveConnections.load(), PoolLabels);
            Oss << FormatMetric("pool_idle_connections", Metrics->PoolStats.IdleConnections.load(), PoolLabels);
            Oss << FormatMetric("pool_max_connections", Metrics->PoolStats.MaxConnections.load(), PoolLabels);
            Oss << FormatMetric("pool_utilization", Metrics->PoolStats.GetPoolUtilization(), PoolLabels);
            Oss << FormatMetric("pool_avg_wait_time_ms", Metrics->PoolStats.GetAverageWaitTimeMs(), PoolLabels);
            Oss << FormatMetric("pool_exhaustion_count", Metrics->PoolStats.PoolExhaustionCount.load(), PoolLabels);
        }
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportOperationMetrics(const std::map<std::string, std::unique_ptr<OperationMetrics>>& Metrics)
    {
        std::ostringstream Oss;
        
        for (const auto& Pair : Metrics) {
            const auto& OperationId = Pair.first;
            const auto& Metrics = Pair.second;
            
            std::map<std::string, std::string> Labels = {
                {"operation_id", OperationId},
                {"operation_type", Metrics->OperationType},
                {"protocol", Metrics->Protocol}
            };
            
            // 基础指标
            Oss << FormatMetric("operation_total_operations", Metrics->TotalOperations.load(), Labels);
            Oss << FormatMetric("operation_successful_operations", Metrics->SuccessfulOperations.load(), Labels);
            Oss << FormatMetric("operation_failed_operations", Metrics->FailedOperations.load(), Labels);
            Oss << FormatMetric("operation_success_rate", Metrics->GetSuccessRate(), Labels);
            
            // 延迟指标
            Oss << FormatMetric("operation_avg_latency_ms", Metrics->GetAverageLatencyMs(), Labels);
            Oss << FormatMetric("operation_min_latency_ms", Metrics->MinLatencyNs.load() / 1'000'000.0, Labels);
            Oss << FormatMetric("operation_max_latency_ms", Metrics->MaxLatencyNs.load() / 1'000'000.0, Labels);
            Oss << FormatMetric("operation_p50_latency_ms", Metrics->GetLatencyPercentileMs(0.50), Labels);
            Oss << FormatMetric("operation_p95_latency_ms", Metrics->GetLatencyPercentileMs(0.95), Labels);
            Oss << FormatMetric("operation_p99_latency_ms", Metrics->GetLatencyPercentileMs(0.99), Labels);
            
            // 吞吐量指标
            Oss << FormatMetric("operation_throughput_ops_per_sec", Metrics->GetThroughputOpsPerSec(), Labels);
            Oss << FormatMetric("operation_total_bytes_processed", Metrics->TotalBytesProcessed.load(), Labels);
            Oss << FormatMetric("operation_total_messages_processed", Metrics->TotalMessagesProcessed.load(), Labels);
            
            // 操作特定指标
            Oss << FormatMetric("operation_partial_operations", Metrics->PartialOperations.load(), Labels);
            Oss << FormatMetric("operation_retry_count", Metrics->RetryCount.load(), Labels);
            Oss << FormatMetric("operation_buffer_overflows", Metrics->BufferOverflows.load(), Labels);
            
            // 错误分类指标
            Oss << FormatMetric("operation_network_errors", Metrics->ErrorStatistics.NetworkErrors.load(), Labels);
            Oss << FormatMetric("operation_timeout_errors", Metrics->ErrorStatistics.TimeoutErrors.load(), Labels);
            Oss << FormatMetric("operation_protocol_errors", Metrics->ErrorStatistics.ProtocolErrors.load(), Labels);
            Oss << FormatMetric("operation_authentication_errors", Metrics->ErrorStatistics.AuthenticationErrors.load(), Labels);
            Oss << FormatMetric("operation_authorization_errors", Metrics->ErrorStatistics.AuthorizationErrors.load(), Labels);
            Oss << FormatMetric("operation_resource_errors", Metrics->ErrorStatistics.ResourceErrors.load(), Labels);
            Oss << FormatMetric("operation_system_errors", Metrics->ErrorStatistics.SystemErrors.load(), Labels);
            Oss << FormatMetric("operation_unknown_errors", Metrics->ErrorStatistics.UnknownErrors.load(), Labels);
        }
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportSystemMetrics(const SystemMetrics& Metrics)
    {
        std::ostringstream Oss;
        
        // 连接统计
        Oss << FormatMetric("system_active_connections", Metrics.ActiveConnections.load());
        Oss << FormatMetric("system_total_connections", Metrics.TotalConnections.load());
        Oss << FormatMetric("system_failed_connections", Metrics.FailedConnections.load());
        
        // 资源使用统计
        Oss << FormatMetric("system_memory_usage_bytes", Metrics.ResourceStats.MemoryUsageBytes.load());
        Oss << FormatMetric("system_peak_memory_usage_bytes", Metrics.ResourceStats.PeakMemoryUsageBytes.load());
        Oss << FormatMetric("system_thread_count", Metrics.ResourceStats.ThreadCount.load());
        Oss << FormatMetric("system_cpu_usage_percent", Metrics.ResourceStats.CpuUsagePercent.load());
        Oss << FormatMetric("system_file_descriptor_count", Metrics.ResourceStats.FileDescriptorCount.load());
        Oss << FormatMetric("system_buffer_pool_usage", Metrics.ResourceStats.BufferPoolUsage.load());
        Oss << FormatMetric("system_buffer_pool_capacity", Metrics.ResourceStats.BufferPoolCapacity.load());
        Oss << FormatMetric("system_buffer_pool_utilization", Metrics.ResourceStats.GetBufferPoolUtilization());
        
        // 事件循环统计
        Oss << FormatMetric("system_event_loop_iterations", Metrics.EventLoopIterations.load());
        Oss << FormatMetric("system_events_processed", Metrics.EventsProcessed.load());
        Oss << FormatMetric("system_idle_time_ms", Metrics.IdleTimeMs.load());
        
        // 批处理统计
        Oss << FormatMetric("system_batch_processing_count", Metrics.BatchProcessingCount.load());
        Oss << FormatMetric("system_average_batch_size", Metrics.AverageBatchSize.load());
        Oss << FormatMetric("system_max_batch_size", Metrics.MaxBatchSize.load());
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportAllMetrics(
        const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& ConnectionMetrics,
        const std::map<std::string, std::unique_ptr<OperationMetrics>>& OperationMetrics,
        const SystemMetrics& SystemMetrics)
    {
        std::ostringstream Oss;
        
        // 添加注释头
        Oss << "# HELP helianthus_network_metrics Helianthus Network Framework Metrics\n";
        Oss << "# TYPE helianthus_network_metrics counter\n\n";
        
        // 导出连接指标
        Oss << "# Connection Metrics\n";
        Oss << ExportConnectionMetrics(ConnectionMetrics);
        Oss << "\n";
        
        // 导出操作指标
        Oss << "# Operation Metrics\n";
        Oss << ExportOperationMetrics(OperationMetrics);
        Oss << "\n";
        
        // 导出系统指标
        Oss << "# System Metrics\n";
        Oss << ExportSystemMetrics(SystemMetrics);
        
        return Oss.str();
    }

    // 性能监控管理器方法
    std::string PerformanceMonitor::ExportPrometheusMetrics() const
    {
        return PrometheusExporter::ExportAllMetrics(
            GetConnectionMetrics(),
            GetOperationMetrics(),
            GetSystemMetrics()
        );
    }

    // 重置统计
    void PerformanceMonitor::ResetAllMetrics()
    {
        {
            std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
            for (auto& Pair : ConnectionMetricsMap) {
                Pair.second->Reset();
            }
        }
        
        {
            std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
            for (auto& Pair : OperationMetricsMap) {
                Pair.second->Reset();
            }
        }
        
        {
            std::lock_guard<std::mutex> Lock(SystemMetricsMutex);
            CurrentSystemMetrics = SystemMetrics{};
        }
    }

    void PerformanceMonitor::ResetConnectionMetrics(const std::string& ConnectionId)
    {
        if (ConnectionId.empty()) {
            std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
            for (auto& Pair : ConnectionMetricsMap) {
                Pair.second->Reset();
            }
        } else {
            std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
            auto It = ConnectionMetricsMap.find(ConnectionId);
            if (It != ConnectionMetricsMap.end()) {
                It->second->Reset();
            }
        }
    }

    void PerformanceMonitor::ResetOperationMetrics(const std::string& OperationId)
    {
        if (OperationId.empty()) {
            std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
            for (auto& Pair : OperationMetricsMap) {
                Pair.second->Reset();
            }
        } else {
            std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
            auto It = OperationMetricsMap.find(OperationId);
            if (It != OperationMetricsMap.end()) {
                It->second->Reset();
            }
        }
    }
}
