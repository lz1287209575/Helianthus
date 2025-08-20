#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "Discovery/DiscoveryTypes.h"
#include "IHealthChecker.h"

namespace Helianthus::Discovery
{
/**
 * @brief 高性能健康检查器实现
 *
 * 提供多种健康检查策略，支持自动检查、手动检查、
 * 熔断器模式和健康度评分系统
 */
class HealthChecker : public IHealthChecker
{
public:
    HealthChecker();
    virtual ~HealthChecker();

    // IHealthChecker 实现
    DiscoveryResult Initialize(const HealthCheckConfig& DefaultConfig) override;
    void Shutdown() override;
    bool IsInitialized() const override;

    // 健康检查注册
    DiscoveryResult RegisterHealthCheck(ServiceInstanceId InstanceId,
                                        const HealthCheckConfig& Config) override;
    DiscoveryResult UpdateHealthCheck(ServiceInstanceId InstanceId,
                                      const HealthCheckConfig& Config) override;
    DiscoveryResult UnregisterHealthCheck(ServiceInstanceId InstanceId) override;
    bool IsHealthCheckRegistered(ServiceInstanceId InstanceId) const override;
    HealthCheckConfig GetHealthCheckConfig(ServiceInstanceId InstanceId) const override;

    // 健康检查执行
    void StartHealthCheck(ServiceInstanceId InstanceId) override;
    void StopHealthCheck(ServiceInstanceId InstanceId) override;
    void StartAllHealthChecks() override;
    void StopAllHealthChecks() override;
    bool IsHealthCheckRunning(ServiceInstanceId InstanceId) const override;

    // 手动健康检查
    HealthScore PerformHealthCheck(ServiceInstanceId InstanceId) override;
    std::future<HealthScore> PerformHealthCheckAsync(ServiceInstanceId InstanceId) override;
    std::unordered_map<ServiceInstanceId, HealthScore>
    PerformBatchHealthCheck(const std::vector<ServiceInstanceId>& InstanceIds) override;

    // 健康状态获取
    HealthScore GetHealthScore(ServiceInstanceId InstanceId) const override;
    ServiceState GetHealthState(ServiceInstanceId InstanceId) const override;
    bool IsHealthy(ServiceInstanceId InstanceId) const override;
    Common::TimestampMs GetLastHealthCheckTime(ServiceInstanceId InstanceId) const override;
    uint32_t GetConsecutiveFailures(ServiceInstanceId InstanceId) const override;
    uint32_t GetConsecutiveSuccesses(ServiceInstanceId InstanceId) const override;

    // 健康检查类型实现
    DiscoveryResult PerformTcpHealthCheck(ServiceInstanceId InstanceId,
                                          const Network::NetworkAddress& Address,
                                          uint32_t TimeoutMs) override;
    DiscoveryResult PerformHttpHealthCheck(ServiceInstanceId InstanceId,
                                           const std::string& Url,
                                           const std::string& ExpectedResponse,
                                           uint32_t TimeoutMs) override;
    DiscoveryResult PerformCustomHealthCheck(ServiceInstanceId InstanceId,
                                             std::function<bool()> HealthCheckFunction) override;
    DiscoveryResult PerformHeartbeatCheck(ServiceInstanceId InstanceId) override;
    DiscoveryResult PerformPingCheck(ServiceInstanceId InstanceId,
                                     const Network::NetworkAddress& Address,
                                     uint32_t TimeoutMs) override;

    // 健康阈值和配置
    void SetHealthThresholds(ServiceInstanceId InstanceId,
                             uint32_t UnhealthyThreshold,
                             uint32_t HealthyThreshold) override;
    void GetHealthThresholds(ServiceInstanceId InstanceId,
                             uint32_t& OutUnhealthyThreshold,
                             uint32_t& OutHealthyThreshold) const override;
    void SetDefaultHealthThresholds(uint32_t UnhealthyThreshold,
                                    uint32_t HealthyThreshold) override;
    void SetHealthCheckInterval(ServiceInstanceId InstanceId, uint32_t IntervalMs) override;
    uint32_t GetHealthCheckInterval(ServiceInstanceId InstanceId) const override;

    // 熔断器集成
    void EnableCircuitBreaker(ServiceInstanceId InstanceId,
                              uint32_t FailureThreshold,
                              uint32_t RecoveryTimeMs) override;
    void DisableCircuitBreaker(ServiceInstanceId InstanceId) override;
    bool IsCircuitBreakerOpen(ServiceInstanceId InstanceId) const override;
    void ResetCircuitBreaker(ServiceInstanceId InstanceId) override;
    Common::TimestampMs GetCircuitBreakerOpenTime(ServiceInstanceId InstanceId) const override;

    // 健康监控和警报
    void SetHealthAlertThreshold(HealthScore Threshold) override;
    HealthScore GetHealthAlertThreshold() const override;
    std::vector<ServiceInstanceId> GetUnhealthyInstances() const override;
    std::vector<ServiceInstanceId> GetCriticalInstances() const override;
    uint32_t GetUnhealthyInstanceCount() const override;

    // 健康检查统计
    uint64_t GetTotalHealthChecks(ServiceInstanceId InstanceId) const override;
    uint64_t GetSuccessfulHealthChecks(ServiceInstanceId InstanceId) const override;
    uint64_t GetFailedHealthChecks(ServiceInstanceId InstanceId) const override;
    float GetHealthCheckSuccessRate(ServiceInstanceId InstanceId) const override;
    uint32_t GetAverageResponseTime(ServiceInstanceId InstanceId) const override;
    uint32_t GetLastResponseTime(ServiceInstanceId InstanceId) const override;

    // 批量操作
    std::vector<ServiceInstanceId> GetHealthyInstances() const override;
    std::unordered_map<ServiceInstanceId, HealthScore> GetAllHealthScores() const override;
    void ResetHealthStats(ServiceInstanceId InstanceId) override;
    void ResetAllHealthStats() override;
    void RefreshAllHealthChecks() override;

    // 配置管理
    void UpdateDefaultConfig(const HealthCheckConfig& Config) override;
    HealthCheckConfig GetDefaultConfig() const override;
    void SetGlobalHealthCheckInterval(uint32_t IntervalMs) override;
    uint32_t GetGlobalHealthCheckInterval() const override;
    void SetGlobalHealthCheckTimeout(uint32_t TimeoutMs) override;
    uint32_t GetGlobalHealthCheckTimeout() const override;

    // 自定义健康检查提供者
    DiscoveryResult RegisterCustomHealthCheckProvider(
        HealthCheckType Type, std::function<HealthScore(ServiceInstanceId)> Provider) override;
    void UnregisterCustomHealthCheckProvider(HealthCheckType Type) override;
    bool IsCustomHealthCheckProviderRegistered(HealthCheckType Type) const override;

    // 健康度衰减和恢复
    void SetHealthDegradationRate(ServiceInstanceId InstanceId, float DegradationRate) override;
    void SetHealthRecoveryRate(ServiceInstanceId InstanceId, float RecoveryRate) override;
    float GetHealthDegradationRate(ServiceInstanceId InstanceId) const override;
    float GetHealthRecoveryRate(ServiceInstanceId InstanceId) const override;
    void UpdateHealthTrend(ServiceInstanceId InstanceId, int32_t TrendDirection) override;

    // 事件回调和通知
    void SetHealthCheckCallback(HealthCheckCallback Callback) override;
    void SetHealthStateChangeCallback(
        std::function<void(ServiceInstanceId, ServiceState, ServiceState)> Callback) override;
    void SetHealthAlertCallback(
        std::function<void(ServiceInstanceId, HealthScore, const std::string&)> Callback) override;
    void RemoveAllCallbacks() override;

    // 调试和诊断
    std::string GetHealthCheckInfo(ServiceInstanceId InstanceId) const override;
    std::vector<std::string> GetHealthCheckLog(ServiceInstanceId InstanceId,
                                               uint32_t MaxEntries = 100) const override;
    void EnableHealthCheckLogging(bool Enable = true) override;
    bool IsHealthCheckLoggingEnabled() const override;
    void SetHealthCheckLogLevel(Common::LogLevel Level) override;

    // 高级健康指标
    void UpdateCustomHealthMetric(ServiceInstanceId InstanceId,
                                  const std::string& MetricName,
                                  float Value) override;
    float GetCustomHealthMetric(ServiceInstanceId InstanceId,
                                const std::string& MetricName) const override;
    std::unordered_map<std::string, float>
    GetAllCustomHealthMetrics(ServiceInstanceId InstanceId) const override;
    void ClearCustomHealthMetrics(ServiceInstanceId InstanceId) override;

    // 健康检查调度
    void PauseHealthCheck(ServiceInstanceId InstanceId) override;
    void ResumeHealthCheck(ServiceInstanceId InstanceId) override;
    bool IsHealthCheckPaused(ServiceInstanceId InstanceId) const override;
    void ScheduleHealthCheck(ServiceInstanceId InstanceId,
                             Common::TimestampMs ScheduleTime) override;
    Common::TimestampMs GetNextHealthCheckTime(ServiceInstanceId InstanceId) const override;

private:
    // 内部数据结构
    struct CircuitBreakerState
    {
        bool Enabled = false;
        bool IsOpen = false;
        uint32_t FailureThreshold = 5;
        uint32_t RecoveryTimeMs = 60000;
        uint32_t ConsecutiveFailures = 0;
        Common::TimestampMs OpenTime = 0;
        Common::TimestampMs LastTryTime = 0;
    };

    struct HealthCheckEntry
    {
        ServiceInstanceId InstanceId;
        HealthCheckConfig Config;
        ServiceState CurrentState = ServiceState::UNKNOWN;
        HealthScore CurrentScore = 0;
        uint32_t ConsecutiveFailures = 0;
        uint32_t ConsecutiveSuccesses = 0;
        Common::TimestampMs LastCheckTime = 0;
        Common::TimestampMs NextCheckTime = 0;
        bool IsRunning = false;
        bool IsPaused = false;

        // 统计信息
        uint64_t TotalChecks = 0;
        uint64_t SuccessfulChecks = 0;
        uint64_t FailedChecks = 0;
        uint32_t LastResponseTime = 0;
        uint64_t TotalResponseTime = 0;

        // 熔断器
        CircuitBreakerState CircuitBreaker;

        // 自定义指标
        std::unordered_map<std::string, float> CustomMetrics;

        // 健康衰减/恢复率
        float DegradationRate = 0.1f;
        float RecoveryRate = 0.05f;

        // 健康检查日志
        std::queue<std::string> HealthLog;
    };

    // 辅助方法
    HealthScore CalculateHealthScore(const HealthCheckEntry& Entry) const;
    ServiceState DetermineServiceState(HealthScore Score,
                                       uint32_t ConsecutiveFailures,
                                       uint32_t ConsecutiveSuccesses) const;
    void
    UpdateHealthEntry(ServiceInstanceId InstanceId, bool CheckSucceeded, uint32_t ResponseTime);
    void
    NotifyHealthChange(ServiceInstanceId InstanceId, ServiceState OldState, ServiceState NewState);
    void
    NotifyHealthAlert(ServiceInstanceId InstanceId, HealthScore Score, const std::string& Message);
    void AddHealthLog(ServiceInstanceId InstanceId, const std::string& LogEntry);

    // 线程相关
    void StartHealthCheckThread();
    void StopHealthCheckThread();
    void HealthCheckLoop();
    void ProcessScheduledChecks();

    // 健康检查执行
    HealthScore ExecuteHealthCheck(const HealthCheckEntry& Entry);
    bool ExecuteTcpCheck(const Network::NetworkAddress& Address, uint32_t TimeoutMs);
    bool ExecuteHttpCheck(const std::string& Url,
                          const std::string& ExpectedResponse,
                          uint32_t TimeoutMs);
    bool ExecutePingCheck(const Network::NetworkAddress& Address, uint32_t TimeoutMs);

private:
    // 配置和状态
    HealthCheckConfig DefaultConfig;
    std::atomic<bool> IsInitializedFlag{false};
    std::atomic<bool> ShuttingDown{false};
    std::atomic<HealthScore> HealthAlertThreshold{20};
    std::atomic<uint32_t> GlobalInterval{30000};
    std::atomic<uint32_t> GlobalTimeout{5000};

    // 健康检查存储
    mutable std::mutex HealthChecksMutex;
    std::unordered_map<ServiceInstanceId, HealthCheckEntry> HealthChecks;

    // 自定义健康检查提供者
    mutable std::mutex ProvidersMutex;
    std::unordered_map<HealthCheckType, std::function<HealthScore(ServiceInstanceId)>>
        CustomProviders;

    // 线程和调度
    std::thread HealthCheckThread;
    std::atomic<bool> StopThread{false};

    // 回调函数
    HealthCheckCallback HealthCallback;
    std::function<void(ServiceInstanceId, ServiceState, ServiceState)> StateChangeCallback;
    std::function<void(ServiceInstanceId, HealthScore, const std::string&)> AlertCallback;

    // 日志
    std::atomic<bool> LoggingEnabled{false};
    Common::LogLevel LogLevel = Common::LogLevel::INFO;
};

}  // namespace Helianthus::Discovery