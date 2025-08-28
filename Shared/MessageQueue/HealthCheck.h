#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Common/Logger.h"
#include "MessageTypes.h"

namespace Helianthus::MessageQueue
{

// 健康检查结果
enum class HealthCheckResult : uint8_t
{
    HEALTHY = 0,        // 健康
    UNHEALTHY = 1,      // 不健康
    DEGRADED = 2,       // 降级
    CRITICAL = 3,       // 严重
    UNKNOWN = 4         // 未知
};

// 健康检查类型
enum class HealthCheckType : uint8_t
{
    QUEUE_HEALTH = 0,           // 队列健康检查
    PERSISTENCE_HEALTH = 1,     // 持久化健康检查
    MEMORY_HEALTH = 2,          // 内存健康检查
    DISK_HEALTH = 3,            // 磁盘健康检查
    NETWORK_HEALTH = 4,         // 网络健康检查
    DATABASE_HEALTH = 5,        // 数据库健康检查
    CUSTOM_HEALTH = 6           // 自定义健康检查
};

// 健康检查状态
struct HealthCheckStatus
{
    HealthCheckResult Result = HealthCheckResult::UNKNOWN;
    std::string Message;
    Common::TimestampMs LastCheckTime = 0;
    Common::TimestampMs LastSuccessTime = 0;
    Common::TimestampMs LastFailureTime = 0;
    uint32_t ConsecutiveFailures = 0;
    uint32_t ConsecutiveSuccesses = 0;
    uint32_t TotalChecks = 0;
    uint32_t TotalFailures = 0;
    float SuccessRate = 0.0f;
    uint32_t ResponseTimeMs = 0;
    std::unordered_map<std::string, std::string> Details;
};

// 健康检查配置
struct HealthCheckConfig
{
    HealthCheckType Type = HealthCheckType::QUEUE_HEALTH;
    uint32_t IntervalMs = 30000;        // 检查间隔（毫秒）
    uint32_t TimeoutMs = 5000;          // 超时时间（毫秒）
    uint32_t UnhealthyThreshold = 3;    // 不健康阈值
    uint32_t HealthyThreshold = 2;      // 健康阈值
    bool Enabled = true;                // 是否启用
    std::string QueueName;              // 队列名称（队列健康检查时使用）
    std::string CustomEndpoint;         // 自定义端点
    std::unordered_map<std::string, std::string> CustomParameters;
};

// 整体健康状态
struct OverallHealthStatus
{
    HealthCheckResult OverallResult = HealthCheckResult::UNKNOWN;
    std::string OverallMessage;
    Common::TimestampMs LastUpdateTime = 0;
    uint32_t TotalChecks = 0;
    uint32_t HealthyChecks = 0;
    uint32_t UnhealthyChecks = 0;
    uint32_t DegradedChecks = 0;
    uint32_t CriticalChecks = 0;
    std::unordered_map<HealthCheckType, HealthCheckStatus> CheckStatuses;
    std::vector<std::string> Issues;
    std::vector<std::string> Warnings;
};

// 健康检查回调函数类型
using HealthCheckCallback = std::function<void(HealthCheckType Type, const HealthCheckStatus& Status)>;
using OverallHealthCallback = std::function<void(const OverallHealthStatus& Status)>;

// 健康检查器接口
class IHealthChecker
{
public:
    virtual ~IHealthChecker() = default;

    // 初始化和生命周期
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // 健康检查配置
    virtual bool RegisterHealthCheck(HealthCheckType Type, const HealthCheckConfig& Config) = 0;
    virtual bool UnregisterHealthCheck(HealthCheckType Type) = 0;
    virtual bool UpdateHealthCheckConfig(HealthCheckType Type, const HealthCheckConfig& Config) = 0;
    virtual HealthCheckConfig GetHealthCheckConfig(HealthCheckType Type) const = 0;
    virtual bool IsHealthCheckRegistered(HealthCheckType Type) const = 0;

    // 健康检查控制
    virtual void StartHealthChecks() = 0;
    virtual void StopHealthChecks() = 0;
    virtual bool AreHealthChecksRunning() const = 0;

    // 手动健康检查
    virtual HealthCheckStatus PerformHealthCheck(HealthCheckType Type) = 0;
    virtual std::future<HealthCheckStatus> PerformHealthCheckAsync(HealthCheckType Type) = 0;
    virtual OverallHealthStatus PerformAllHealthChecks() = 0;

    // 健康状态查询
    virtual HealthCheckStatus GetHealthStatus(HealthCheckType Type) const = 0;
    virtual OverallHealthStatus GetOverallHealthStatus() const = 0;
    virtual bool IsHealthy(HealthCheckType Type) const = 0;
    virtual bool IsOverallHealthy() const = 0;

    // 回调设置
    virtual void SetHealthCheckCallback(HealthCheckCallback Callback) = 0;
    virtual void SetOverallHealthCallback(OverallHealthCallback Callback) = 0;
    virtual void RemoveCallbacks() = 0;

    // 统计信息
    virtual void ResetStatistics() = 0;
    virtual void EnableLogging(bool Enable = true) = 0;
    virtual bool IsLoggingEnabled() const = 0;
};

// 健康检查器实现
class HealthChecker : public IHealthChecker
{
public:
    HealthChecker();
    virtual ~HealthChecker();

    // IHealthChecker 实现
    bool Initialize() override;
    void Shutdown() override;
    bool IsInitialized() const override;

    // 健康检查配置
    bool RegisterHealthCheck(HealthCheckType Type, const HealthCheckConfig& Config) override;
    bool UnregisterHealthCheck(HealthCheckType Type) override;
    bool UpdateHealthCheckConfig(HealthCheckType Type, const HealthCheckConfig& Config) override;
    HealthCheckConfig GetHealthCheckConfig(HealthCheckType Type) const override;
    bool IsHealthCheckRegistered(HealthCheckType Type) const override;

    // 健康检查控制
    void StartHealthChecks() override;
    void StopHealthChecks() override;
    bool AreHealthChecksRunning() const override;

    // 手动健康检查
    HealthCheckStatus PerformHealthCheck(HealthCheckType Type) override;
    std::future<HealthCheckStatus> PerformHealthCheckAsync(HealthCheckType Type) override;
    OverallHealthStatus PerformAllHealthChecks() override;

    // 健康状态查询
    HealthCheckStatus GetHealthStatus(HealthCheckType Type) const override;
    OverallHealthStatus GetOverallHealthStatus() const override;
    bool IsHealthy(HealthCheckType Type) const override;
    bool IsOverallHealthy() const override;

    // 回调设置
    void SetHealthCheckCallback(HealthCheckCallback Callback) override;
    void SetOverallHealthCallback(OverallHealthCallback Callback) override;
    void RemoveCallbacks() override;

    // 统计信息
    void ResetStatistics() override;
    void EnableLogging(bool Enable = true) override;
    bool IsLoggingEnabled() const override;

private:
    // 内部数据结构
    struct HealthCheckEntry
    {
        HealthCheckConfig Config;
        HealthCheckStatus Status;
        std::chrono::steady_clock::time_point LastCheckTime;
        bool IsRunning = false;
    };

    // 成员变量
    std::atomic<bool> Initialized{false};
    std::atomic<bool> Running{false};
    std::atomic<bool> LoggingEnabled{true};
    std::thread HealthCheckThread;
    mutable std::shared_mutex HealthCheckMutex;
    std::unordered_map<HealthCheckType, HealthCheckEntry> HealthChecks;
    OverallHealthStatus OverallStatus;
    
    // 回调函数
    HealthCheckCallback HealthCheckCallbackFunc;
    OverallHealthCallback OverallHealthCallbackFunc;

    // 内部方法
    void HealthCheckLoop();
    HealthCheckStatus PerformQueueHealthCheck(const HealthCheckConfig& Config);
    HealthCheckStatus PerformPersistenceHealthCheck(const HealthCheckConfig& Config);
    HealthCheckStatus PerformMemoryHealthCheck(const HealthCheckConfig& Config);
    HealthCheckStatus PerformDiskHealthCheck(const HealthCheckConfig& Config);
    HealthCheckStatus PerformNetworkHealthCheck(const HealthCheckConfig& Config);
    HealthCheckStatus PerformDatabaseHealthCheck(const HealthCheckConfig& Config);
    HealthCheckStatus PerformCustomHealthCheck(const HealthCheckConfig& Config);
    
    void UpdateOverallHealthStatus();
    void NotifyHealthCheckCallback(HealthCheckType Type, const HealthCheckStatus& Status);
    void NotifyOverallHealthCallback(const OverallHealthStatus& Status);
    std::string HealthCheckResultToString(HealthCheckResult Result) const;
    std::string HealthCheckTypeToString(HealthCheckType Type) const;
};

// 全局健康检查器实例
extern std::unique_ptr<HealthChecker> GlobalHealthChecker;

// 便捷函数
HealthChecker& GetHealthChecker();
bool InitializeHealthChecker();
void ShutdownHealthChecker();

} // namespace Helianthus::MessageQueue
