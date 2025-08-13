#pragma once

#include "DiscoveryTypes.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <future>

namespace Helianthus::Discovery
{
    /**
     * @brief Abstract interface for health checker
     * 
     * The health checker monitors the availability and performance of
     * service instances through various health check mechanisms including
     * TCP connections, HTTP requests, custom protocols, and heartbeats.
     */
    class IHealthChecker
    {
    public:
        virtual ~IHealthChecker() = default;

        // Initialization and lifecycle
        virtual DiscoveryResult Initialize(const HealthCheckConfig& DefaultConfig) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsInitialized() const = 0;

        // Health check registration
        virtual DiscoveryResult RegisterHealthCheck(ServiceInstanceId InstanceId, const HealthCheckConfig& Config) = 0;
        virtual DiscoveryResult UpdateHealthCheck(ServiceInstanceId InstanceId, const HealthCheckConfig& Config) = 0;
        virtual DiscoveryResult UnregisterHealthCheck(ServiceInstanceId InstanceId) = 0;
        virtual bool IsHealthCheckRegistered(ServiceInstanceId InstanceId) const = 0;
        virtual HealthCheckConfig GetHealthCheckConfig(ServiceInstanceId InstanceId) const = 0;

        // Health check execution
        virtual void StartHealthCheck(ServiceInstanceId InstanceId) = 0;
        virtual void StopHealthCheck(ServiceInstanceId InstanceId) = 0;
        virtual void StartAllHealthChecks() = 0;
        virtual void StopAllHealthChecks() = 0;
        virtual bool IsHealthCheckRunning(ServiceInstanceId InstanceId) const = 0;

        // Manual health checks
        virtual HealthScore PerformHealthCheck(ServiceInstanceId InstanceId) = 0;
        virtual std::future<HealthScore> PerformHealthCheckAsync(ServiceInstanceId InstanceId) = 0;
        virtual std::unordered_map<ServiceInstanceId, HealthScore> PerformBatchHealthCheck(const std::vector<ServiceInstanceId>& InstanceIds) = 0;

        // Health status retrieval
        virtual HealthScore GetHealthScore(ServiceInstanceId InstanceId) const = 0;
        virtual ServiceState GetHealthState(ServiceInstanceId InstanceId) const = 0;
        virtual bool IsHealthy(ServiceInstanceId InstanceId) const = 0;
        virtual Common::TimestampMs GetLastHealthCheckTime(ServiceInstanceId InstanceId) const = 0;
        virtual uint32_t GetConsecutiveFailures(ServiceInstanceId InstanceId) const = 0;
        virtual uint32_t GetConsecutiveSuccesses(ServiceInstanceId InstanceId) const = 0;

        // Health check types and implementations
        virtual DiscoveryResult PerformTcpHealthCheck(ServiceInstanceId InstanceId, const Network::NetworkAddress& Address, uint32_t TimeoutMs) = 0;
        virtual DiscoveryResult PerformHttpHealthCheck(ServiceInstanceId InstanceId, const std::string& Url, const std::string& ExpectedResponse, uint32_t TimeoutMs) = 0;
        virtual DiscoveryResult PerformCustomHealthCheck(ServiceInstanceId InstanceId, std::function<bool()> HealthCheckFunction) = 0;
        virtual DiscoveryResult PerformHeartbeatCheck(ServiceInstanceId InstanceId) = 0;
        virtual DiscoveryResult PerformPingCheck(ServiceInstanceId InstanceId, const Network::NetworkAddress& Address, uint32_t TimeoutMs) = 0;

        // Health thresholds and configuration
        virtual void SetHealthThresholds(ServiceInstanceId InstanceId, uint32_t UnhealthyThreshold, uint32_t HealthyThreshold) = 0;
        virtual void GetHealthThresholds(ServiceInstanceId InstanceId, uint32_t& OutUnhealthyThreshold, uint32_t& OutHealthyThreshold) const = 0;
        virtual void SetDefaultHealthThresholds(uint32_t UnhealthyThreshold, uint32_t HealthyThreshold) = 0;
        virtual void SetHealthCheckInterval(ServiceInstanceId InstanceId, uint32_t IntervalMs) = 0;
        virtual uint32_t GetHealthCheckInterval(ServiceInstanceId InstanceId) const = 0;

        // Circuit breaker integration
        virtual void EnableCircuitBreaker(ServiceInstanceId InstanceId, uint32_t FailureThreshold, uint32_t RecoveryTimeMs) = 0;
        virtual void DisableCircuitBreaker(ServiceInstanceId InstanceId) = 0;
        virtual bool IsCircuitBreakerOpen(ServiceInstanceId InstanceId) const = 0;
        virtual void ResetCircuitBreaker(ServiceInstanceId InstanceId) = 0;
        virtual Common::TimestampMs GetCircuitBreakerOpenTime(ServiceInstanceId InstanceId) const = 0;

        // Health monitoring and alerts
        virtual void SetHealthAlertThreshold(HealthScore Threshold) = 0;
        virtual HealthScore GetHealthAlertThreshold() const = 0;
        virtual std::vector<ServiceInstanceId> GetUnhealthyInstances() const = 0;
        virtual std::vector<ServiceInstanceId> GetCriticalInstances() const = 0;
        virtual uint32_t GetUnhealthyInstanceCount() const = 0;

        // Health check statistics
        virtual uint64_t GetTotalHealthChecks(ServiceInstanceId InstanceId) const = 0;
        virtual uint64_t GetSuccessfulHealthChecks(ServiceInstanceId InstanceId) const = 0;
        virtual uint64_t GetFailedHealthChecks(ServiceInstanceId InstanceId) const = 0;
        virtual float GetHealthCheckSuccessRate(ServiceInstanceId InstanceId) const = 0;
        virtual uint32_t GetAverageResponseTime(ServiceInstanceId InstanceId) const = 0;
        virtual uint32_t GetLastResponseTime(ServiceInstanceId InstanceId) const = 0;

        // Bulk operations
        virtual std::vector<ServiceInstanceId> GetHealthyInstances() const = 0;
        virtual std::unordered_map<ServiceInstanceId, HealthScore> GetAllHealthScores() const = 0;
        virtual void ResetHealthStats(ServiceInstanceId InstanceId) = 0;
        virtual void ResetAllHealthStats() = 0;
        virtual void RefreshAllHealthChecks() = 0;

        // Configuration management
        virtual void UpdateDefaultConfig(const HealthCheckConfig& Config) = 0;
        virtual HealthCheckConfig GetDefaultConfig() const = 0;
        virtual void SetGlobalHealthCheckInterval(uint32_t IntervalMs) = 0;
        virtual uint32_t GetGlobalHealthCheckInterval() const = 0;
        virtual void SetGlobalHealthCheckTimeout(uint32_t TimeoutMs) = 0;
        virtual uint32_t GetGlobalHealthCheckTimeout() const = 0;

        // Custom health check providers
        virtual DiscoveryResult RegisterCustomHealthCheckProvider(HealthCheckType Type, std::function<HealthScore(ServiceInstanceId)> Provider) = 0;
        virtual void UnregisterCustomHealthCheckProvider(HealthCheckType Type) = 0;
        virtual bool IsCustomHealthCheckProviderRegistered(HealthCheckType Type) const = 0;

        // Health degradation and recovery
        virtual void SetHealthDegradationRate(ServiceInstanceId InstanceId, float DegradationRate) = 0;
        virtual void SetHealthRecoveryRate(ServiceInstanceId InstanceId, float RecoveryRate) = 0;
        virtual float GetHealthDegradationRate(ServiceInstanceId InstanceId) const = 0;
        virtual float GetHealthRecoveryRate(ServiceInstanceId InstanceId) const = 0;
        virtual void UpdateHealthTrend(ServiceInstanceId InstanceId, int32_t TrendDirection) = 0;

        // Event callbacks and notifications
        virtual void SetHealthCheckCallback(HealthCheckCallback Callback) = 0;
        virtual void SetHealthStateChangeCallback(std::function<void(ServiceInstanceId, ServiceState, ServiceState)> Callback) = 0;
        virtual void SetHealthAlertCallback(std::function<void(ServiceInstanceId, HealthScore, const std::string&)> Callback) = 0;
        virtual void RemoveAllCallbacks() = 0;

        // Debugging and diagnostics
        virtual std::string GetHealthCheckInfo(ServiceInstanceId InstanceId) const = 0;
        virtual std::vector<std::string> GetHealthCheckLog(ServiceInstanceId InstanceId, uint32_t MaxEntries = 100) const = 0;
        virtual void EnableHealthCheckLogging(bool Enable = true) = 0;
        virtual bool IsHealthCheckLoggingEnabled() const = 0;
        virtual void SetHealthCheckLogLevel(Common::LogLevel Level) = 0;

        // Advanced health metrics
        virtual void UpdateCustomHealthMetric(ServiceInstanceId InstanceId, const std::string& MetricName, float Value) = 0;
        virtual float GetCustomHealthMetric(ServiceInstanceId InstanceId, const std::string& MetricName) const = 0;
        virtual std::unordered_map<std::string, float> GetAllCustomHealthMetrics(ServiceInstanceId InstanceId) const = 0;
        virtual void ClearCustomHealthMetrics(ServiceInstanceId InstanceId) = 0;

        // Health check scheduling
        virtual void PauseHealthCheck(ServiceInstanceId InstanceId) = 0;
        virtual void ResumeHealthCheck(ServiceInstanceId InstanceId) = 0;
        virtual bool IsHealthCheckPaused(ServiceInstanceId InstanceId) const = 0;
        virtual void ScheduleHealthCheck(ServiceInstanceId InstanceId, Common::TimestampMs ScheduleTime) = 0;
        virtual Common::TimestampMs GetNextHealthCheckTime(ServiceInstanceId InstanceId) const = 0;
    };

} // namespace Helianthus::Discovery
