#include "Shared/Network/Asio/PerformanceMetrics.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Helianthus::Network::Asio
{
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
        if (It == ConnectionMetricsMap.end()) return;

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
    }

    void PerformanceMonitor::UpdateOperationMetrics(const std::string& OperationId, 
                                                   bool Success, uint64_t LatencyNs,
                                                   uint64_t BytesProcessed)
    {
        std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
        auto It = OperationMetricsMap.find(OperationId);
        if (It == OperationMetricsMap.end()) return;

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

    // 系统统计
    void PerformanceMonitor::UpdateSystemMetrics(const SystemMetrics& Metrics)
    {
        std::lock_guard<std::mutex> Lock(SystemMetricsMutex);
        CurrentSystemMetrics = Metrics;
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

    // 导出 Prometheus 格式
    std::string PerformanceMonitor::ExportPrometheusMetrics() const
    {
        return PrometheusExporter::ExportAllMetrics(
            GetConnectionMetrics(), GetOperationMetrics(), GetSystemMetrics());
    }

    // 重置统计
    void PerformanceMonitor::ResetAllMetrics()
    {
        ResetConnectionMetrics();
        ResetOperationMetrics();
        
        // 重置系统指标
        std::lock_guard<std::mutex> Lock(SystemMetricsMutex);
        CurrentSystemMetrics = SystemMetrics();
    }

    void PerformanceMonitor::ResetConnectionMetrics(const std::string& ConnectionId)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMetricsMutex);
        if (ConnectionId.empty()) {
            for (auto& [Id, Metrics] : ConnectionMetricsMap) {
                Metrics->Reset();
            }
        } else {
            auto It = ConnectionMetricsMap.find(ConnectionId);
            if (It != ConnectionMetricsMap.end()) {
                It->second->Reset();
            }
        }
    }

    void PerformanceMonitor::ResetOperationMetrics(const std::string& OperationId)
    {
        std::lock_guard<std::mutex> Lock(OperationMetricsMutex);
        if (OperationId.empty()) {
            for (auto& [Id, Metrics] : OperationMetricsMap) {
                Metrics->Reset();
            }
        } else {
            auto It = OperationMetricsMap.find(OperationId);
            if (It != OperationMetricsMap.end()) {
                It->second->Reset();
            }
        }
    }

    // PrometheusExporter 实现
    std::string PrometheusExporter::FormatMetric(const std::string& Name, double Value, 
                                                const std::map<std::string, std::string>& Labels)
    {
        std::ostringstream Oss;
        Oss << Name;
        
        if (!Labels.empty()) {
            Oss << "{";
            bool First = true;
            for (const auto& [Key, Value] : Labels) {
                if (!First) Oss << ",";
                Oss << Key << "=\"" << Value << "\"";
                First = false;
            }
            Oss << "}";
        }
        
        Oss << " " << std::fixed << std::setprecision(6) << Value << "\n";
        return Oss.str();
    }

    std::string PrometheusExporter::ExportConnectionMetrics(const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& Metrics)
    {
        std::ostringstream Oss;
        Oss << "# HELP helianthus_connection_total_operations Total operations per connection\n";
        Oss << "# TYPE helianthus_connection_total_operations counter\n";
        
        for (const auto& [Id, Metrics] : Metrics) {
            std::map<std::string, std::string> Labels = {
                {"connection_id", Id},
                {"remote_address", Metrics->RemoteAddress}
            };
            
            Oss << FormatMetric("helianthus_connection_total_operations", 
                               static_cast<double>(Metrics->TotalOperations.load()), Labels);
            Oss << FormatMetric("helianthus_connection_successful_operations", 
                               static_cast<double>(Metrics->SuccessfulOperations.load()), Labels);
            Oss << FormatMetric("helianthus_connection_failed_operations", 
                               static_cast<double>(Metrics->FailedOperations.load()), Labels);
            Oss << FormatMetric("helianthus_connection_success_rate", 
                               Metrics->GetSuccessRate(), Labels);
            Oss << FormatMetric("helianthus_connection_average_latency_ms", 
                               Metrics->GetAverageLatencyMs(), Labels);
            Oss << FormatMetric("helianthus_connection_throughput_ops_per_sec", 
                               Metrics->GetThroughputOpsPerSec(), Labels);
            Oss << FormatMetric("helianthus_connection_duration_seconds", 
                               Metrics->GetConnectionDurationSec(), Labels);
        }
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportOperationMetrics(const std::map<std::string, std::unique_ptr<OperationMetrics>>& Metrics)
    {
        std::ostringstream Oss;
        Oss << "# HELP helianthus_operation_total_operations Total operations by type\n";
        Oss << "# TYPE helianthus_operation_total_operations counter\n";
        
        for (const auto& [Id, Metrics] : Metrics) {
            std::map<std::string, std::string> Labels = {
                {"operation_id", Id},
                {"operation_type", Metrics->OperationType},
                {"protocol", Metrics->Protocol}
            };
            
            Oss << FormatMetric("helianthus_operation_total_operations", 
                               static_cast<double>(Metrics->TotalOperations.load()), Labels);
            Oss << FormatMetric("helianthus_operation_successful_operations", 
                               static_cast<double>(Metrics->SuccessfulOperations.load()), Labels);
            Oss << FormatMetric("helianthus_operation_failed_operations", 
                               static_cast<double>(Metrics->FailedOperations.load()), Labels);
            Oss << FormatMetric("helianthus_operation_success_rate", 
                               Metrics->GetSuccessRate(), Labels);
            Oss << FormatMetric("helianthus_operation_average_latency_ms", 
                               Metrics->GetAverageLatencyMs(), Labels);
            Oss << FormatMetric("helianthus_operation_throughput_ops_per_sec", 
                               Metrics->GetThroughputOpsPerSec(), Labels);
        }
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportSystemMetrics(const SystemMetrics& Metrics)
    {
        std::ostringstream Oss;
        Oss << "# HELP helianthus_system_active_connections Active connections\n";
        Oss << "# TYPE helianthus_system_active_connections gauge\n";
        
        Oss << FormatMetric("helianthus_system_active_connections", 
                           static_cast<double>(Metrics.ActiveConnections.load()));
        Oss << FormatMetric("helianthus_system_total_connections", 
                           static_cast<double>(Metrics.TotalConnections.load()));
        Oss << FormatMetric("helianthus_system_failed_connections", 
                           static_cast<double>(Metrics.FailedConnections.load()));
        Oss << FormatMetric("helianthus_system_memory_usage_bytes", 
                           static_cast<double>(Metrics.MemoryUsageBytes.load()));
        Oss << FormatMetric("helianthus_system_thread_count", 
                           static_cast<double>(Metrics.ThreadCount.load()));
        Oss << FormatMetric("helianthus_system_cpu_usage_percent", 
                           static_cast<double>(Metrics.CpuUsagePercent.load()));
        Oss << FormatMetric("helianthus_system_event_loop_iterations", 
                           static_cast<double>(Metrics.EventLoopIterations.load()));
        Oss << FormatMetric("helianthus_system_events_processed", 
                           static_cast<double>(Metrics.EventsProcessed.load()));
        Oss << FormatMetric("helianthus_system_idle_time_ms", 
                           static_cast<double>(Metrics.IdleTimeMs.load()));
        Oss << FormatMetric("helianthus_system_batch_processing_count", 
                           static_cast<double>(Metrics.BatchProcessingCount.load()));
        Oss << FormatMetric("helianthus_system_average_batch_size", 
                           static_cast<double>(Metrics.AverageBatchSize.load()));
        Oss << FormatMetric("helianthus_system_max_batch_size", 
                           static_cast<double>(Metrics.MaxBatchSize.load()));
        
        return Oss.str();
    }

    std::string PrometheusExporter::ExportAllMetrics(
        const std::map<std::string, std::unique_ptr<ConnectionMetrics>>& ConnectionMetrics,
        const std::map<std::string, std::unique_ptr<OperationMetrics>>& OperationMetrics,
        const SystemMetrics& SystemMetrics)
    {
        std::ostringstream Oss;
        Oss << "# Helianthus Network Performance Metrics\n";
        Oss << "# Generated at: " << std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() << "\n\n";
        
        Oss << ExportConnectionMetrics(ConnectionMetrics);
        Oss << "\n";
        Oss << ExportOperationMetrics(OperationMetrics);
        Oss << "\n";
        Oss << ExportSystemMetrics(SystemMetrics);
        
        return Oss.str();
    }
}
