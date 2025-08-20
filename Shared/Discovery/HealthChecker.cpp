#include "HealthChecker.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>

#include "Discovery/DiscoveryTypes.h"

namespace Helianthus::Discovery
{
HealthChecker::HealthChecker()
{
    DefaultConfig = HealthCheckConfig{};
    std::cout << "[HealthChecker] Health checker initialized" << std::endl;
}

HealthChecker::~HealthChecker()
{
    Shutdown();
}

DiscoveryResult HealthChecker::Initialize(const HealthCheckConfig& DefaultConfig)
{
    if (IsInitializedFlag)
    {
        return DiscoveryResult::INTERNAL_ERROR;
    }

    this->DefaultConfig = DefaultConfig;
    GlobalInterval = DefaultConfig.IntervalMs;
    GlobalTimeout = DefaultConfig.TimeoutMs;

    IsInitializedFlag = true;
    ShuttingDown = false;

    StartHealthCheckThread();

    std::cout << "[HealthChecker] Init Finish, Global Check Tick Time: " << GlobalInterval.load()
              << "ms" << std::endl;

    return DiscoveryResult::SUCCESS;
}

void HealthChecker::Shutdown()
{
    if (!IsInitializedFlag)
    {
        return;
    }

    std::cout << "[HealthChecker] Start Shutdown" << std::endl;

    ShuttingDown = true;
    StopHealthCheckThread();

    {
        std::lock_guard<std::mutex> Lock(HealthChecksMutex);
        HealthChecks.clear();
    }

    {
        std::lock_guard<std::mutex> Lock(ProvidersMutex);
        CustomProviders.clear();
    }

    IsInitializedFlag = false;
    std::cout << "[HealthChecker] Shutdown Finish" << std::endl;
}

bool HealthChecker::IsInitialized() const
{
    return IsInitializedFlag;
}

DiscoveryResult HealthChecker::RegisterHealthCheck(ServiceInstanceId InstanceId,
                                                   const HealthCheckConfig& Config)
{
    if (!IsInitializedFlag || InstanceId == InvalidServiceInstanceId)
    {
        return DiscoveryResult::INVALID_SERVICE_INFO;
    }

    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    if (HealthChecks.find(InstanceId) != HealthChecks.end())
    {
        return DiscoveryResult::SERVICE_ALREADY_REGISTERED;
    }

    HealthCheckEntry Entry;
    Entry.InstanceId = InstanceId;
    Entry.Config = Config;
    Entry.CurrentState = ServiceState::UNKNOWN;
    Entry.CurrentScore = MaxHealthScore / 2;  // 初始中等健康度
    Entry.NextCheckTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count() +
                          Config.IntervalMs;

    HealthChecks[InstanceId] = Entry;

    std::cout << "[HealthChecker] Register Service Health Check: " << InstanceId
              << ", Type: " << static_cast<int>(Config.Type) << ", Tick Time: " << Config.IntervalMs
              << "ms" << std::endl;

    return DiscoveryResult::SUCCESS;
}

DiscoveryResult HealthChecker::UpdateHealthCheck(ServiceInstanceId InstanceId,
                                                 const HealthCheckConfig& Config)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It == HealthChecks.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    It->second.Config = Config;
    std::cout << "[HealthChecker] Update Service Health Check Configuration: " << InstanceId
              << std::endl;

    return DiscoveryResult::SUCCESS;
}

DiscoveryResult HealthChecker::UnregisterHealthCheck(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It == HealthChecks.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    HealthChecks.erase(It);
    std::cout << "[HealthChecker] Cancel Service Health Check: " << InstanceId << std::endl;

    return DiscoveryResult::SUCCESS;
}

bool HealthChecker::IsHealthCheckRegistered(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);
    return HealthChecks.find(InstanceId) != HealthChecks.end();
}

HealthCheckConfig HealthChecker::GetHealthCheckConfig(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        return It->second.Config;
    }

    return DefaultConfig;
}

void HealthChecker::StartHealthCheck(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        It->second.IsRunning = true;
        It->second.IsPaused = false;
        std::cout << "[HealthChecker] Start Healthy Check: " << InstanceId << std::endl;
    }
}

void HealthChecker::StopHealthCheck(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        It->second.IsRunning = false;
        std::cout << "[HealthChecker] Stop Healthy Check: " << InstanceId << std::endl;
    }
}

void HealthChecker::StartAllHealthChecks()
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    for (auto& [InstanceId, Entry] : HealthChecks)
    {
        Entry.IsRunning = true;
        Entry.IsPaused = false;
    }

    std::cout << "[HealthChecker] Start All Healthy Check" << std::endl;
}

void HealthChecker::StopAllHealthChecks()
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    for (auto& [InstanceId, Entry] : HealthChecks)
    {
        Entry.IsRunning = false;
    }

    std::cout << "[HealthChecker] Stop All Healthy Check" << std::endl;
}

bool HealthChecker::IsHealthCheckRunning(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() && It->second.IsRunning;
}

HealthScore HealthChecker::PerformHealthCheck(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It == HealthChecks.end())
    {
        return 0;
    }

    return ExecuteHealthCheck(It->second);
}

std::future<HealthScore> HealthChecker::PerformHealthCheckAsync(ServiceInstanceId InstanceId)
{
    auto Promise = std::make_shared<std::promise<HealthScore>>();
    auto Future = Promise->get_future();

    std::thread(
        [this, InstanceId, Promise]()
        {
            HealthScore Score = PerformHealthCheck(InstanceId);
            Promise->set_value(Score);
        })
        .detach();

    return Future;
}

std::unordered_map<ServiceInstanceId, HealthScore>
HealthChecker::PerformBatchHealthCheck(const std::vector<ServiceInstanceId>& InstanceIds)
{
    std::unordered_map<ServiceInstanceId, HealthScore> Results;

    for (ServiceInstanceId InstanceId : InstanceIds)
    {
        Results[InstanceId] = PerformHealthCheck(InstanceId);
    }

    return Results;
}

HealthScore HealthChecker::GetHealthScore(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() ? It->second.CurrentScore : 0;
}

ServiceState HealthChecker::GetHealthState(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() ? It->second.CurrentState : ServiceState::UNKNOWN;
}

bool HealthChecker::IsHealthy(ServiceInstanceId InstanceId) const
{
    ServiceState State = GetHealthState(InstanceId);
    return State == ServiceState::HEALTHY;
}

Common::TimestampMs HealthChecker::GetLastHealthCheckTime(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() ? It->second.LastCheckTime : 0;
}

uint32_t HealthChecker::GetConsecutiveFailures(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() ? It->second.ConsecutiveFailures : 0;
}

uint32_t HealthChecker::GetConsecutiveSuccesses(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() ? It->second.ConsecutiveSuccesses : 0;
}

DiscoveryResult HealthChecker::PerformTcpHealthCheck(ServiceInstanceId InstanceId,
                                                     const Network::NetworkAddress& Address,
                                                     uint32_t TimeoutMs)
{
    bool Success = ExecuteTcpCheck(Address, TimeoutMs);
    UpdateHealthEntry(InstanceId, Success, TimeoutMs);

    return Success ? DiscoveryResult::SUCCESS : DiscoveryResult::FAILED;
}

DiscoveryResult HealthChecker::PerformHttpHealthCheck(ServiceInstanceId InstanceId,
                                                      const std::string& Url,
                                                      const std::string& ExpectedResponse,
                                                      uint32_t TimeoutMs)
{
    bool Success = ExecuteHttpCheck(Url, ExpectedResponse, TimeoutMs);
    UpdateHealthEntry(InstanceId, Success, TimeoutMs);

    return Success ? DiscoveryResult::SUCCESS : DiscoveryResult::FAILED;
}

DiscoveryResult HealthChecker::PerformCustomHealthCheck(ServiceInstanceId InstanceId,
                                                        std::function<bool()> HealthCheckFunction)
{
    if (!HealthCheckFunction)
    {
        return DiscoveryResult::INVALID_SERVICE_INFO;
    }

    auto StartTime = std::chrono::steady_clock::now();
    bool Success = false;

    try
    {
        Success = HealthCheckFunction();
    }
    catch (const std::exception& e)
    {
        AddHealthLog(InstanceId, "Custom Health Check Exception: " + std::string(e.what()));
    }

    auto EndTime = std::chrono::steady_clock::now();
    uint32_t ResponseTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime).count();

    UpdateHealthEntry(InstanceId, Success, ResponseTime);
    return Success ? DiscoveryResult::SUCCESS : DiscoveryResult::FAILED;
}

DiscoveryResult HealthChecker::PerformHeartbeatCheck(ServiceInstanceId InstanceId)
{
    // 简化的心跳检查 - 检查最后心跳时间
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It == HealthChecks.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    uint32_t TimeSinceLastCheck = Now - It->second.LastCheckTime;
    bool IsHealthy = TimeSinceLastCheck < (It->second.Config.IntervalMs * 2);

    return IsHealthy ? DiscoveryResult::SUCCESS : DiscoveryResult::FAILED;
}

DiscoveryResult HealthChecker::PerformPingCheck(ServiceInstanceId InstanceId,
                                                const Network::NetworkAddress& Address,
                                                uint32_t TimeoutMs)
{
    bool Success = ExecutePingCheck(Address, TimeoutMs);
    UpdateHealthEntry(InstanceId, Success, TimeoutMs);

    return Success ? DiscoveryResult::SUCCESS : DiscoveryResult::FAILED;
}

// 私有辅助方法实现

void HealthChecker::StartHealthCheckThread()
{
    StopThread = false;
    HealthCheckThread = std::thread(&HealthChecker::HealthCheckLoop, this);
    std::cout << "[HealthChecker] Health Check Thread Start" << std::endl;
}

void HealthChecker::StopHealthCheckThread()
{
    StopThread = true;

    if (HealthCheckThread.joinable())
    {
        HealthCheckThread.join();
    }

    std::cout << "[HealthChecker] Health Check Thread Stop" << std::endl;
}

void HealthChecker::HealthCheckLoop()
{
    while (!StopThread && !ShuttingDown)
    {
        ProcessScheduledChecks();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 每秒检查一次
    }
}

void HealthChecker::ProcessScheduledChecks()
{
    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    for (auto& [InstanceId, Entry] : HealthChecks)
    {
        if (!Entry.IsRunning || Entry.IsPaused)
        {
            continue;
        }

        if (Now >= Entry.NextCheckTime)
        {
            // 执行健康检查
            HealthScore Score = ExecuteHealthCheck(Entry);

            // 更新下次检查时间
            Entry.NextCheckTime = Now + Entry.Config.IntervalMs;

            // 记录日志
            if (LoggingEnabled)
            {
                std::stringstream LogMsg;
                LogMsg << "Health Check Finish - InstanceId: " << InstanceId << ", Score: " << Score
                       << ", CurrentState: " << static_cast<int>(Entry.CurrentState);
                AddHealthLog(InstanceId, LogMsg.str());
            }
        }
    }
}

HealthScore HealthChecker::ExecuteHealthCheck(const HealthCheckEntry& Entry)
{
    // 这里应该根据配置类型执行实际的健康检查
    // 为演示目的，使用随机分数
    static thread_local std::random_device RandomDevice;
    static thread_local std::mt19937 Generator(RandomDevice());
    static thread_local std::uniform_int_distribution<HealthScore> Distribution(50, 100);

    return Distribution(Generator);
}

void HealthChecker::UpdateHealthEntry(ServiceInstanceId InstanceId,
                                      bool CheckSucceeded,
                                      uint32_t ResponseTime)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);

    auto It = HealthChecks.find(InstanceId);
    if (It == HealthChecks.end())
    {
        return;
    }

    HealthCheckEntry& Entry = It->second;
    ServiceState OldState = Entry.CurrentState;

    Entry.LastCheckTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    Entry.LastResponseTime = ResponseTime;
    Entry.TotalChecks++;
    Entry.TotalResponseTime += ResponseTime;

    if (CheckSucceeded)
    {
        Entry.SuccessfulChecks++;
        Entry.ConsecutiveSuccesses++;
        Entry.ConsecutiveFailures = 0;

        // 提高健康分数
        Entry.CurrentScore = std::min(MaxHealthScore, Entry.CurrentScore + 10);
    }
    else
    {
        Entry.FailedChecks++;
        Entry.ConsecutiveFailures++;
        Entry.ConsecutiveSuccesses = 0;

        // 降低健康分数
        Entry.CurrentScore = Entry.CurrentScore > 10 ? Entry.CurrentScore - 10 : 0;
    }

    // 更新服务状态
    Entry.CurrentState = DetermineServiceState(
        Entry.CurrentScore, Entry.ConsecutiveFailures, Entry.ConsecutiveSuccesses);

    // 通知状态变化
    if (OldState != Entry.CurrentState && StateChangeCallback)
    {
        StateChangeCallback(InstanceId, OldState, Entry.CurrentState);
    }
}

ServiceState HealthChecker::DetermineServiceState(HealthScore Score,
                                                  uint32_t ConsecutiveFailures,
                                                  uint32_t ConsecutiveSuccesses) const
{
    if (Score >= 80 && ConsecutiveFailures == 0)
    {
        return ServiceState::HEALTHY;
    }
    else if (Score >= 50 && ConsecutiveFailures < 3)
    {
        return ServiceState::HEALTHY;
    }
    else if (Score >= 20)
    {
        return ServiceState::UNHEALTHY;
    }
    else
    {
        return ServiceState::CRITICAL;
    }
}

bool HealthChecker::ExecuteTcpCheck(const Network::NetworkAddress& Address, uint32_t TimeoutMs)
{
    // 简化的TCP连接检查实现
    // 在实际实现中，这里应该尝试建立TCP连接
    return Address.IsValid();
}

bool HealthChecker::ExecuteHttpCheck(const std::string& Url,
                                     const std::string& ExpectedResponse,
                                     uint32_t TimeoutMs)
{
    // 简化的HTTP检查实现
    // 在实际实现中，这里应该发送HTTP请求并检查响应
    return !Url.empty();
}

bool HealthChecker::ExecutePingCheck(const Network::NetworkAddress& Address, uint32_t TimeoutMs)
{
    // 简化的Ping检查实现
    // 在实际实现中，这里应该发送ICMP ping请求
    return Address.IsValid();
}

void HealthChecker::AddHealthLog(ServiceInstanceId InstanceId, const std::string& LogEntry)
{
    if (!LoggingEnabled)
    {
        return;
    }

    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        It->second.HealthLog.push(LogEntry);

        // 限制日志大小
        if (It->second.HealthLog.size() > 100)
        {
            It->second.HealthLog.pop();
        }
    }
}

// 简化实现其他必需的方法
void HealthChecker::SetHealthThresholds(ServiceInstanceId InstanceId,
                                        uint32_t UnhealthyThreshold,
                                        uint32_t HealthyThreshold)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);
    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        It->second.Config.UnhealthyThreshold = UnhealthyThreshold;
        It->second.Config.HealthyThreshold = HealthyThreshold;
    }
}

void HealthChecker::GetHealthThresholds(ServiceInstanceId InstanceId,
                                        uint32_t& OutUnhealthyThreshold,
                                        uint32_t& OutHealthyThreshold) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);
    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        OutUnhealthyThreshold = It->second.Config.UnhealthyThreshold;
        OutHealthyThreshold = It->second.Config.HealthyThreshold;
    }
    else
    {
        OutUnhealthyThreshold = DefaultConfig.UnhealthyThreshold;
        OutHealthyThreshold = DefaultConfig.HealthyThreshold;
    }
}

void HealthChecker::SetDefaultHealthThresholds(uint32_t UnhealthyThreshold,
                                               uint32_t HealthyThreshold)
{
    DefaultConfig.UnhealthyThreshold = UnhealthyThreshold;
    DefaultConfig.HealthyThreshold = HealthyThreshold;
}

void HealthChecker::SetHealthCheckInterval(ServiceInstanceId InstanceId, uint32_t IntervalMs)
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);
    auto It = HealthChecks.find(InstanceId);
    if (It != HealthChecks.end())
    {
        It->second.Config.IntervalMs = IntervalMs;
    }
}

uint32_t HealthChecker::GetHealthCheckInterval(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(HealthChecksMutex);
    auto It = HealthChecks.find(InstanceId);
    return It != HealthChecks.end() ? It->second.Config.IntervalMs : DefaultConfig.IntervalMs;
}

// 其他简化实现的方法...
void HealthChecker::EnableCircuitBreaker(ServiceInstanceId InstanceId,
                                         uint32_t FailureThreshold,
                                         uint32_t RecoveryTimeMs)
{
}
void HealthChecker::DisableCircuitBreaker(ServiceInstanceId InstanceId) {}
bool HealthChecker::IsCircuitBreakerOpen(ServiceInstanceId InstanceId) const
{
    return false;
}
void HealthChecker::ResetCircuitBreaker(ServiceInstanceId InstanceId) {}
Common::TimestampMs HealthChecker::GetCircuitBreakerOpenTime(ServiceInstanceId InstanceId) const
{
    return 0;
}
void HealthChecker::SetHealthAlertThreshold(HealthScore Threshold)
{
    HealthAlertThreshold = Threshold;
}
HealthScore HealthChecker::GetHealthAlertThreshold() const
{
    return HealthAlertThreshold;
}
std::vector<ServiceInstanceId> HealthChecker::GetUnhealthyInstances() const
{
    return {};
}
std::vector<ServiceInstanceId> HealthChecker::GetCriticalInstances() const
{
    return {};
}
uint32_t HealthChecker::GetUnhealthyInstanceCount() const
{
    return 0;
}
uint64_t HealthChecker::GetTotalHealthChecks(ServiceInstanceId InstanceId) const
{
    return 0;
}
uint64_t HealthChecker::GetSuccessfulHealthChecks(ServiceInstanceId InstanceId) const
{
    return 0;
}
uint64_t HealthChecker::GetFailedHealthChecks(ServiceInstanceId InstanceId) const
{
    return 0;
}
float HealthChecker::GetHealthCheckSuccessRate(ServiceInstanceId InstanceId) const
{
    return 0.0f;
}
uint32_t HealthChecker::GetAverageResponseTime(ServiceInstanceId InstanceId) const
{
    return 0;
}
uint32_t HealthChecker::GetLastResponseTime(ServiceInstanceId InstanceId) const
{
    return 0;
}
std::vector<ServiceInstanceId> HealthChecker::GetHealthyInstances() const
{
    return {};
}
std::unordered_map<ServiceInstanceId, HealthScore> HealthChecker::GetAllHealthScores() const
{
    return {};
}
void HealthChecker::ResetHealthStats(ServiceInstanceId InstanceId) {}
void HealthChecker::ResetAllHealthStats() {}
void HealthChecker::RefreshAllHealthChecks() {}
void HealthChecker::UpdateDefaultConfig(const HealthCheckConfig& Config)
{
    DefaultConfig = Config;
}
HealthCheckConfig HealthChecker::GetDefaultConfig() const
{
    return DefaultConfig;
}
void HealthChecker::SetGlobalHealthCheckInterval(uint32_t IntervalMs)
{
    GlobalInterval = IntervalMs;
}
uint32_t HealthChecker::GetGlobalHealthCheckInterval() const
{
    return GlobalInterval;
}
void HealthChecker::SetGlobalHealthCheckTimeout(uint32_t TimeoutMs)
{
    GlobalTimeout = TimeoutMs;
}
uint32_t HealthChecker::GetGlobalHealthCheckTimeout() const
{
    return GlobalTimeout;
}
DiscoveryResult HealthChecker::RegisterCustomHealthCheckProvider(
    HealthCheckType Type, std::function<HealthScore(ServiceInstanceId)> Provider)
{
    return DiscoveryResult::SUCCESS;
}
void HealthChecker::UnregisterCustomHealthCheckProvider(HealthCheckType Type) {}
bool HealthChecker::IsCustomHealthCheckProviderRegistered(HealthCheckType Type) const
{
    return false;
}
void HealthChecker::SetHealthDegradationRate(ServiceInstanceId InstanceId, float DegradationRate) {}
void HealthChecker::SetHealthRecoveryRate(ServiceInstanceId InstanceId, float RecoveryRate) {}
float HealthChecker::GetHealthDegradationRate(ServiceInstanceId InstanceId) const
{
    return 0.1f;
}
float HealthChecker::GetHealthRecoveryRate(ServiceInstanceId InstanceId) const
{
    return 0.05f;
}
void HealthChecker::UpdateHealthTrend(ServiceInstanceId InstanceId, int32_t TrendDirection) {}
void HealthChecker::SetHealthCheckCallback(HealthCheckCallback Callback)
{
    HealthCallback = Callback;
}
void HealthChecker::SetHealthStateChangeCallback(
    std::function<void(ServiceInstanceId, ServiceState, ServiceState)> Callback)
{
    StateChangeCallback = Callback;
}
void HealthChecker::SetHealthAlertCallback(
    std::function<void(ServiceInstanceId, HealthScore, const std::string&)> Callback)
{
    AlertCallback = Callback;
}
void HealthChecker::RemoveAllCallbacks()
{
    HealthCallback = nullptr;
    StateChangeCallback = nullptr;
    AlertCallback = nullptr;
}
std::string HealthChecker::GetHealthCheckInfo(ServiceInstanceId InstanceId) const
{
    return "";
}
std::vector<std::string> HealthChecker::GetHealthCheckLog(ServiceInstanceId InstanceId,
                                                          uint32_t MaxEntries) const
{
    return {};
}
void HealthChecker::EnableHealthCheckLogging(bool Enable)
{
    LoggingEnabled = Enable;
}
bool HealthChecker::IsHealthCheckLoggingEnabled() const
{
    return LoggingEnabled;
}
void HealthChecker::SetHealthCheckLogLevel(Common::LogLevel Level)
{
    LogLevel = Level;
}
void HealthChecker::UpdateCustomHealthMetric(ServiceInstanceId InstanceId,
                                             const std::string& MetricName,
                                             float Value)
{
}
float HealthChecker::GetCustomHealthMetric(ServiceInstanceId InstanceId,
                                           const std::string& MetricName) const
{
    return 0.0f;
}
std::unordered_map<std::string, float>
HealthChecker::GetAllCustomHealthMetrics(ServiceInstanceId InstanceId) const
{
    return {};
}
void HealthChecker::ClearCustomHealthMetrics(ServiceInstanceId InstanceId) {}
void HealthChecker::PauseHealthCheck(ServiceInstanceId InstanceId) {}
void HealthChecker::ResumeHealthCheck(ServiceInstanceId InstanceId) {}
bool HealthChecker::IsHealthCheckPaused(ServiceInstanceId InstanceId) const
{
    return false;
}
void HealthChecker::ScheduleHealthCheck(ServiceInstanceId InstanceId,
                                        Common::TimestampMs ScheduleTime)
{
}
Common::TimestampMs HealthChecker::GetNextHealthCheckTime(ServiceInstanceId InstanceId) const
{
    return 0;
}

}  // namespace Helianthus::Discovery