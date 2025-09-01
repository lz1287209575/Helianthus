#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>

namespace Helianthus::Monitoring
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
    
    // 获取直方图桶
    std::vector<std::pair<double, uint64_t>> GetHistogramBuckets() const;
    
private:
    std::deque<uint64_t> Samples;
    size_t MaxSamples;
    mutable std::mutex SamplesMutex;
    
    // 预定义的直方图桶（毫秒）
    static const std::vector<double> HistogramBuckets;
};

// 批处理性能统计
struct BatchPerformanceStats
{
    std::atomic<uint64_t> TotalBatches{0};
    std::atomic<uint64_t> TotalMessages{0};
    std::atomic<uint64_t> TotalDurationNs{0};
    std::atomic<uint64_t> MinDurationNs{UINT64_MAX};
    std::atomic<uint64_t> MaxDurationNs{0};
    
    mutable std::unique_ptr<LatencyHistogram> DurationHistogramPtr;
    mutable std::mutex HistogramMutex;
    
    void AddSample(uint64_t DurationNs, uint64_t MessageCount);
    double GetAverageDurationMs() const;
    double GetP50DurationMs() const;
    double GetP95DurationMs() const;
    double GetP99DurationMs() const;
    std::vector<std::pair<double, uint64_t>> GetDurationHistogram() const;
    void Reset();
};

// 零拷贝性能统计
struct ZeroCopyPerformanceStats
{
    std::atomic<uint64_t> TotalOperations{0};
    std::atomic<uint64_t> TotalDurationNs{0};
    std::atomic<uint64_t> MinDurationNs{UINT64_MAX};
    std::atomic<uint64_t> MaxDurationNs{0};
    
    mutable std::unique_ptr<LatencyHistogram> DurationHistogramPtr;
    mutable std::mutex HistogramMutex;
    
    void AddSample(uint64_t DurationNs);
    double GetAverageDurationMs() const;
    double GetP50DurationMs() const;
    double GetP95DurationMs() const;
    double GetP99DurationMs() const;
    std::vector<std::pair<double, uint64_t>> GetDurationHistogram() const;
    void Reset();
};

// 事务性能统计
struct TransactionPerformanceStats
{
    std::atomic<uint64_t> TotalTransactions{0};
    std::atomic<uint64_t> CommittedTransactions{0};
    std::atomic<uint64_t> RolledBackTransactions{0};
    std::atomic<uint64_t> TimeoutTransactions{0};
    std::atomic<uint64_t> FailedTransactions{0};
    
    // 提交时间统计
    std::atomic<uint64_t> TotalCommitTimeNs{0};
    std::atomic<uint64_t> MinCommitTimeNs{UINT64_MAX};
    std::atomic<uint64_t> MaxCommitTimeNs{0};
    
    // 回滚时间统计
    std::atomic<uint64_t> TotalRollbackTimeNs{0};
    std::atomic<uint64_t> MinRollbackTimeNs{UINT64_MAX};
    std::atomic<uint64_t> MaxRollbackTimeNs{0};
    
    mutable std::unique_ptr<LatencyHistogram> CommitTimeHistogramPtr;
    mutable std::unique_ptr<LatencyHistogram> RollbackTimeHistogramPtr;
    mutable std::mutex HistogramMutex;
    
    void AddCommitSample(uint64_t DurationNs);
    void AddRollbackSample(uint64_t DurationNs);
    void UpdateTransactionCount(bool Committed, bool RolledBack, bool Timeout, bool Failed);
    
    double GetAverageCommitTimeMs() const;
    double GetP50CommitTimeMs() const;
    double GetP95CommitTimeMs() const;
    double GetP99CommitTimeMs() const;
    
    double GetAverageRollbackTimeMs() const;
    double GetP50RollbackTimeMs() const;
    double GetP95RollbackTimeMs() const;
    double GetP99RollbackTimeMs() const;
    
    std::vector<std::pair<double, uint64_t>> GetCommitTimeHistogram() const;
    std::vector<std::pair<double, uint64_t>> GetRollbackTimeHistogram() const;
    
    double GetSuccessRate() const;
    double GetRollbackRate() const;
    double GetTimeoutRate() const;
    double GetFailureRate() const;
    
    void Reset();
};

// 增强的 Prometheus 导出器
class EnhancedPrometheusExporter
{
public:
    using MetricsProvider = std::function<std::string()>;
    
    EnhancedPrometheusExporter();
    ~EnhancedPrometheusExporter();
    
    // 基础功能
    bool Start(uint16_t Port, MetricsProvider Provider);
    void Stop();
    bool IsRunning() const { return Running.load(); }
    
    // 性能统计管理
    void UpdateBatchPerformance(const std::string& QueueName, uint64_t DurationNs, uint64_t MessageCount);
    void UpdateZeroCopyPerformance(uint64_t DurationNs);
    void UpdateTransactionPerformance(bool Committed, bool RolledBack, bool Timeout, bool Failed, 
                                    uint64_t CommitTimeNs = 0, uint64_t RollbackTimeNs = 0);
    
    // 获取统计
    const BatchPerformanceStats& GetBatchStats(const std::string& QueueName) const;
    const ZeroCopyPerformanceStats& GetZeroCopyStats() const;
    const TransactionPerformanceStats& GetTransactionStats() const;
    
    // 导出功能
    std::string ExportBatchMetrics() const;
    std::string ExportZeroCopyMetrics() const;
    std::string ExportTransactionMetrics() const;
    std::string ExportAllEnhancedMetrics() const;
    
    // 重置统计
    void ResetBatchStats(const std::string& QueueName = "");
    void ResetZeroCopyStats();
    void ResetTransactionStats();
    void ResetAllStats();

private:
    void ServerLoop(uint16_t Port);
    
    // 格式化函数
    static std::string FormatMetric(const std::string& Name, double Value, 
                                   const std::map<std::string, std::string>& Labels = {});
    static std::string FormatHistogram(const std::string& Name, 
                                      const std::vector<std::pair<double, uint64_t>>& Buckets,
                                      const std::map<std::string, std::string>& Labels = {});
    static std::string FormatCounter(const std::string& Name, uint64_t Value,
                                    const std::map<std::string, std::string>& Labels = {});
    
    MetricsProvider Provider;
    std::thread ServerThread;
    std::atomic<bool> Running{false};
    
    // 性能统计
    mutable std::map<std::string, BatchPerformanceStats> BatchStatsByQueue;
    mutable std::mutex BatchStatsMutex;
    
    ZeroCopyPerformanceStats ZeroCopyStats;
    mutable std::mutex ZeroCopyStatsMutex;
    
    TransactionPerformanceStats TransactionStats;
    mutable std::mutex TransactionStatsMutex;
};

} // namespace Helianthus::Monitoring
