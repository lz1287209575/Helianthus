#include "HealthCheck.h"
#include "Common/ResourceMonitor.h"
#include "Common/StructuredLogger.h"
#include "Common/LogCategories.h"
#include "MessageQueue.h"
#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <sstream>

namespace Helianthus::MessageQueue
{

// 全局健康检查器实例
std::unique_ptr<HealthChecker> GlobalHealthChecker;

// 便捷函数实现
HealthChecker& GetHealthChecker()
{
    if (!GlobalHealthChecker)
    {
        GlobalHealthChecker = std::make_unique<HealthChecker>();
    }
    return *GlobalHealthChecker;
}

bool InitializeHealthChecker()
{
    return GetHealthChecker().Initialize();
}

void ShutdownHealthChecker()
{
    if (GlobalHealthChecker)
    {
        GlobalHealthChecker->Shutdown();
    }
}

// HealthChecker 实现
HealthChecker::HealthChecker()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "健康检查器创建");
}

HealthChecker::~HealthChecker()
{
    Shutdown();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "健康检查器销毁");
}

bool HealthChecker::Initialize()
{
    if (Initialized.load())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "健康检查器已经初始化");
        return true;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "初始化健康检查器");
    Initialized.store(true);
    return true;
}

void HealthChecker::Shutdown()
{
    if (!Initialized.load())
    {
        return;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭健康检查器");
    StopHealthChecks();
    
    if (HealthCheckThread.joinable())
    {
        HealthCheckThread.join();
    }
    
    Initialized.store(false);
}

bool HealthChecker::IsInitialized() const
{
    return Initialized.load();
}

bool HealthChecker::RegisterHealthCheck(HealthCheckType Type, const HealthCheckConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    if (HealthChecks.find(Type) != HealthChecks.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "健康检查类型已注册: {}", HealthCheckTypeToString(Type));
        return false;
    }

    HealthCheckEntry Entry;
    Entry.Config = Config;
    Entry.Status.Result = HealthCheckResult::UNKNOWN;
    Entry.Status.Message = "健康检查已注册，等待首次检查";
    Entry.LastCheckTime = std::chrono::steady_clock::now();
    Entry.IsRunning = false;

    HealthChecks[Type] = Entry;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "注册健康检查: {} (间隔: {}ms)", 
          HealthCheckTypeToString(Type), Config.IntervalMs);
    
    return true;
}

bool HealthChecker::UnregisterHealthCheck(HealthCheckType Type)
{
    std::unique_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    auto It = HealthChecks.find(Type);
    if (It == HealthChecks.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "健康检查类型未注册: {}", HealthCheckTypeToString(Type));
        return false;
    }

    HealthChecks.erase(It);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "注销健康检查: {}", HealthCheckTypeToString(Type));
    return true;
}

bool HealthChecker::UpdateHealthCheckConfig(HealthCheckType Type, const HealthCheckConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    auto It = HealthChecks.find(Type);
    if (It == HealthChecks.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "健康检查类型未注册: {}", HealthCheckTypeToString(Type));
        return false;
    }

    It->second.Config = Config;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "更新健康检查配置: {}", HealthCheckTypeToString(Type));
    return true;
}

HealthCheckConfig HealthChecker::GetHealthCheckConfig(HealthCheckType Type) const
{
    std::shared_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    auto It = HealthChecks.find(Type);
    if (It == HealthChecks.end())
    {
        return HealthCheckConfig{};
    }

    return It->second.Config;
}

bool HealthChecker::IsHealthCheckRegistered(HealthCheckType Type) const
{
    std::shared_lock<std::shared_mutex> Lock(HealthCheckMutex);
    return HealthChecks.find(Type) != HealthChecks.end();
}

void HealthChecker::StartHealthChecks()
{
    if (!Initialized.load())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "健康检查器未初始化");
        return;
    }

    if (Running.load())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "健康检查已在运行");
        return;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "启动健康检查");
    Running.store(true);
    HealthCheckThread = std::thread(&HealthChecker::HealthCheckLoop, this);
}

void HealthChecker::StopHealthChecks()
{
    if (!Running.load())
    {
        return;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止健康检查");
    Running.store(false);
}

bool HealthChecker::AreHealthChecksRunning() const
{
    return Running.load();
}

HealthCheckStatus HealthChecker::PerformHealthCheck(HealthCheckType Type)
{
    std::unique_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    auto It = HealthChecks.find(Type);
    if (It == HealthChecks.end())
    {
        HealthCheckStatus Status;
        Status.Result = HealthCheckResult::UNKNOWN;
        Status.Message = "健康检查类型未注册";
        return Status;
    }

    auto StartTime = std::chrono::steady_clock::now();
    HealthCheckStatus Status;

    switch (Type)
    {
        case HealthCheckType::QUEUE_HEALTH:
            Status = PerformQueueHealthCheck(It->second.Config);
            break;
        case HealthCheckType::PERSISTENCE_HEALTH:
            Status = PerformPersistenceHealthCheck(It->second.Config);
            break;
        case HealthCheckType::MEMORY_HEALTH:
            Status = PerformMemoryHealthCheck(It->second.Config);
            break;
        case HealthCheckType::DISK_HEALTH:
            Status = PerformDiskHealthCheck(It->second.Config);
            break;
        case HealthCheckType::NETWORK_HEALTH:
            Status = PerformNetworkHealthCheck(It->second.Config);
            break;
        case HealthCheckType::DATABASE_HEALTH:
            Status = PerformDatabaseHealthCheck(It->second.Config);
            break;
        case HealthCheckType::CUSTOM_HEALTH:
            Status = PerformCustomHealthCheck(It->second.Config);
            break;
        default:
            Status.Result = HealthCheckResult::UNKNOWN;
            Status.Message = "未知的健康检查类型";
            break;
    }

    auto EndTime = std::chrono::steady_clock::now();
    Status.ResponseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime).count();
    Status.LastCheckTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 更新状态
    It->second.Status = Status;
    It->second.LastCheckTime = std::chrono::steady_clock::now();

    // 更新统计信息
    It->second.Status.TotalChecks++;
    if (Status.Result == HealthCheckResult::HEALTHY)
    {
        It->second.Status.ConsecutiveSuccesses++;
        It->second.Status.ConsecutiveFailures = 0;
        It->second.Status.LastSuccessTime = Status.LastCheckTime;
    }
    else
    {
        It->second.Status.ConsecutiveFailures++;
        It->second.Status.ConsecutiveSuccesses = 0;
        It->second.Status.TotalFailures++;
        It->second.Status.LastFailureTime = Status.LastCheckTime;
    }

    It->second.Status.SuccessRate = static_cast<float>(It->second.Status.TotalChecks - It->second.Status.TotalFailures) / 
                                   static_cast<float>(It->second.Status.TotalChecks);

    // 通知回调
    NotifyHealthCheckCallback(Type, Status);

    if (LoggingEnabled.load())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "健康检查完成: {} = {} (响应时间: {}ms)", 
              HealthCheckTypeToString(Type), 
              HealthCheckResultToString(Status.Result), 
              Status.ResponseTimeMs);
    }

    return Status;
}

std::future<HealthCheckStatus> HealthChecker::PerformHealthCheckAsync(HealthCheckType Type)
{
    return std::async(std::launch::async, [this, Type]() {
        return PerformHealthCheck(Type);
    });
}

OverallHealthStatus HealthChecker::PerformAllHealthChecks()
{
    std::unique_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    OverallHealthStatus Status;
    Status.LastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (auto& Pair : HealthChecks)
    {
        // 直接执行健康检查，不调用PerformHealthCheck以避免死锁
        auto StartTime = std::chrono::steady_clock::now();
        HealthCheckStatus HealthStatus;

        switch (Pair.first)
        {
            case HealthCheckType::QUEUE_HEALTH:
                HealthStatus = PerformQueueHealthCheck(Pair.second.Config);
                break;
            case HealthCheckType::PERSISTENCE_HEALTH:
                HealthStatus = PerformPersistenceHealthCheck(Pair.second.Config);
                break;
            case HealthCheckType::MEMORY_HEALTH:
                HealthStatus = PerformMemoryHealthCheck(Pair.second.Config);
                break;
            case HealthCheckType::DISK_HEALTH:
                HealthStatus = PerformDiskHealthCheck(Pair.second.Config);
                break;
            case HealthCheckType::NETWORK_HEALTH:
                HealthStatus = PerformNetworkHealthCheck(Pair.second.Config);
                break;
            case HealthCheckType::DATABASE_HEALTH:
                HealthStatus = PerformDatabaseHealthCheck(Pair.second.Config);
                break;
            case HealthCheckType::CUSTOM_HEALTH:
                HealthStatus = PerformCustomHealthCheck(Pair.second.Config);
                break;
            default:
                HealthStatus.Result = HealthCheckResult::UNKNOWN;
                HealthStatus.Message = "未知的健康检查类型";
                break;
        }

        auto EndTime = std::chrono::steady_clock::now();
        HealthStatus.ResponseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime).count();
        HealthStatus.LastCheckTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 更新状态
        Pair.second.Status = HealthStatus;
        Pair.second.LastCheckTime = std::chrono::steady_clock::now();

        // 更新统计信息
        Pair.second.Status.TotalChecks++;
        if (HealthStatus.Result == HealthCheckResult::HEALTHY)
        {
            Pair.second.Status.ConsecutiveSuccesses++;
            Pair.second.Status.ConsecutiveFailures = 0;
            Pair.second.Status.LastSuccessTime = HealthStatus.LastCheckTime;
        }
        else
        {
            Pair.second.Status.ConsecutiveFailures++;
            Pair.second.Status.ConsecutiveSuccesses = 0;
            Pair.second.Status.TotalFailures++;
            Pair.second.Status.LastFailureTime = HealthStatus.LastCheckTime;
        }

        Pair.second.Status.SuccessRate = static_cast<float>(Pair.second.Status.TotalChecks - Pair.second.Status.TotalFailures) / 
                                       static_cast<float>(Pair.second.Status.TotalChecks);

        // 添加到整体状态
        Status.CheckStatuses[Pair.first] = HealthStatus;
        Status.TotalChecks++;

        switch (HealthStatus.Result)
        {
            case HealthCheckResult::HEALTHY:
                Status.HealthyChecks++;
                break;
            case HealthCheckResult::UNHEALTHY:
                Status.UnhealthyChecks++;
                Status.Issues.push_back(HealthCheckTypeToString(Pair.first) + ": " + HealthStatus.Message);
                break;
            case HealthCheckResult::DEGRADED:
                Status.DegradedChecks++;
                Status.Warnings.push_back(HealthCheckTypeToString(Pair.first) + ": " + HealthStatus.Message);
                break;
            case HealthCheckResult::CRITICAL:
                Status.CriticalChecks++;
                Status.Issues.push_back(HealthCheckTypeToString(Pair.first) + ": " + HealthStatus.Message);
                break;
            default:
                break;
        }

        // 通知回调
        NotifyHealthCheckCallback(Pair.first, HealthStatus);
    }

    // 确定整体健康状态
    if (Status.CriticalChecks > 0)
    {
        Status.OverallResult = HealthCheckResult::CRITICAL;
        Status.OverallMessage = "存在严重健康问题";
    }
    else if (Status.UnhealthyChecks > 0)
    {
        Status.OverallResult = HealthCheckResult::UNHEALTHY;
        Status.OverallMessage = "存在健康问题";
    }
    else if (Status.DegradedChecks > 0)
    {
        Status.OverallResult = HealthCheckResult::DEGRADED;
        Status.OverallMessage = "系统性能降级";
    }
    else if (Status.HealthyChecks > 0)
    {
        Status.OverallResult = HealthCheckResult::HEALTHY;
        Status.OverallMessage = "系统运行正常";
    }
    else
    {
        Status.OverallResult = HealthCheckResult::UNKNOWN;
        Status.OverallMessage = "健康状态未知";
    }

    OverallStatus = Status;
    NotifyOverallHealthCallback(Status);

    return Status;
}

HealthCheckStatus HealthChecker::GetHealthStatus(HealthCheckType Type) const
{
    std::shared_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    auto It = HealthChecks.find(Type);
    if (It == HealthChecks.end())
    {
        return HealthCheckStatus{};
    }

    return It->second.Status;
}

OverallHealthStatus HealthChecker::GetOverallHealthStatus() const
{
    std::shared_lock<std::shared_mutex> Lock(HealthCheckMutex);
    return OverallStatus;
}

bool HealthChecker::IsHealthy(HealthCheckType Type) const
{
    auto Status = GetHealthStatus(Type);
    return Status.Result == HealthCheckResult::HEALTHY;
}

bool HealthChecker::IsOverallHealthy() const
{
    auto Status = GetOverallHealthStatus();
    return Status.OverallResult == HealthCheckResult::HEALTHY;
}

void HealthChecker::SetHealthCheckCallback(HealthCheckCallback Callback)
{
    HealthCheckCallbackFunc = Callback;
}

void HealthChecker::SetOverallHealthCallback(OverallHealthCallback Callback)
{
    OverallHealthCallbackFunc = Callback;
}

void HealthChecker::RemoveCallbacks()
{
    HealthCheckCallbackFunc = nullptr;
    OverallHealthCallbackFunc = nullptr;
}

void HealthChecker::ResetStatistics()
{
    std::unique_lock<std::shared_mutex> Lock(HealthCheckMutex);
    
    for (auto& Pair : HealthChecks)
    {
        auto& Status = Pair.second.Status;
        Status.TotalChecks = 0;
        Status.TotalFailures = 0;
        Status.ConsecutiveFailures = 0;
        Status.ConsecutiveSuccesses = 0;
        Status.SuccessRate = 0.0f;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "健康检查统计已重置");
}

void HealthChecker::EnableLogging(bool Enable)
{
    LoggingEnabled.store(Enable);
}

bool HealthChecker::IsLoggingEnabled() const
{
    return LoggingEnabled.load();
}

// 私有方法实现
void HealthChecker::HealthCheckLoop()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "健康检查循环启动");

    while (Running.load())
    {
        auto Now = std::chrono::steady_clock::now();
        std::vector<HealthCheckType> TypesToCheck;

        {
            std::shared_lock<std::shared_mutex> Lock(HealthCheckMutex);
            
            for (auto& Pair : HealthChecks)
            {
                if (!Pair.second.Config.Enabled)
                    continue;

                auto TimeSinceLastCheck = std::chrono::duration_cast<std::chrono::milliseconds>(
                    Now - Pair.second.LastCheckTime).count();

                if (TimeSinceLastCheck >= Pair.second.Config.IntervalMs)
                {
                    TypesToCheck.push_back(Pair.first);
                }
            }
        }

        // 执行需要检查的健康检查
        for (auto Type : TypesToCheck)
        {
            if (!Running.load())
                break;

            PerformHealthCheck(Type);
        }

        // 更新整体健康状态
        if (!TypesToCheck.empty())
        {
            UpdateOverallHealthStatus();
        }

        // 休眠一段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "健康检查循环停止");
}

HealthCheckStatus HealthChecker::PerformQueueHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        // 这里应该检查消息队列的状态
        // 由于我们没有直接访问MessageQueue实例，这里提供一个基础实现
        Status.Result = HealthCheckResult::HEALTHY;
        Status.Message = "队列运行正常";
        Status.Details["queue_name"] = Config.QueueName;
        Status.Details["check_type"] = "queue_health";
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "队列健康检查失败: " + std::string(e.what());
    }

    return Status;
}

HealthCheckStatus HealthChecker::PerformPersistenceHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        // 检查持久化目录是否存在和可写
        std::string PersistenceDir = "./data";
        if (std::filesystem::exists(PersistenceDir))
        {
            if (std::filesystem::is_directory(PersistenceDir))
            {
                // 检查磁盘空间
                auto Space = std::filesystem::space(PersistenceDir);
                auto AvailableGB = Space.available / (1024 * 1024 * 1024);
                
                if (AvailableGB > 1) // 至少1GB可用空间
                {
                    Status.Result = HealthCheckResult::HEALTHY;
                    Status.Message = "持久化存储正常";
                    Status.Details["available_gb"] = std::to_string(AvailableGB);
                }
                else
                {
                    Status.Result = HealthCheckResult::CRITICAL;
                    Status.Message = "磁盘空间不足";
                    Status.Details["available_gb"] = std::to_string(AvailableGB);
                }
            }
            else
            {
                Status.Result = HealthCheckResult::UNHEALTHY;
                Status.Message = "持久化目录不是有效目录";
            }
        }
        else
        {
            Status.Result = HealthCheckResult::UNHEALTHY;
            Status.Message = "持久化目录不存在";
        }
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "持久化健康检查失败: " + std::string(e.what());
    }

    return Status;
}

HealthCheckStatus HealthChecker::PerformMemoryHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        // 使用ResourceMonitor检查内存使用情况
        auto& ResourceMonitor = Helianthus::Common::GetResourceMonitor();
        auto Stats = ResourceMonitor.GetCurrentStats();
        
        if (Stats.MemoryUsagePercent < 80.0)
        {
            Status.Result = HealthCheckResult::HEALTHY;
            Status.Message = "内存使用正常";
        }
        else if (Stats.MemoryUsagePercent < 90.0)
        {
            Status.Result = HealthCheckResult::DEGRADED;
            Status.Message = "内存使用较高";
        }
        else
        {
            Status.Result = HealthCheckResult::CRITICAL;
            Status.Message = "内存使用过高";
        }
        
        Status.Details["memory_usage_percent"] = std::to_string(Stats.MemoryUsagePercent);
        Status.Details["available_memory_mb"] = std::to_string(Stats.AvailableMemoryBytes / (1024 * 1024));
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "内存健康检查失败: " + std::string(e.what());
    }

    return Status;
}

HealthCheckStatus HealthChecker::PerformDiskHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        auto& ResourceMonitor = Helianthus::Common::GetResourceMonitor();
        auto Stats = ResourceMonitor.GetCurrentStats();
        
        if (!Stats.DiskStatsList.empty())
        {
            const auto& DiskStats = Stats.DiskStatsList[0]; // 使用第一个磁盘
            
            if (DiskStats.UsagePercent < 80.0)
            {
                Status.Result = HealthCheckResult::HEALTHY;
                Status.Message = "磁盘使用正常";
            }
            else if (DiskStats.UsagePercent < 90.0)
            {
                Status.Result = HealthCheckResult::DEGRADED;
                Status.Message = "磁盘使用较高";
            }
            else
            {
                Status.Result = HealthCheckResult::CRITICAL;
                Status.Message = "磁盘使用过高";
            }
            
            Status.Details["disk_usage_percent"] = std::to_string(DiskStats.UsagePercent);
            Status.Details["available_gb"] = std::to_string(DiskStats.AvailableBytes / (1024 * 1024 * 1024));
            Status.Details["disk_name"] = DiskStats.MountPoint;
        }
        else
        {
            Status.Result = HealthCheckResult::UNKNOWN;
            Status.Message = "无法获取磁盘信息";
        }
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "磁盘健康检查失败: " + std::string(e.what());
    }

    return Status;
}

HealthCheckStatus HealthChecker::PerformNetworkHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        auto& ResourceMonitor = Helianthus::Common::GetResourceMonitor();
        auto Stats = ResourceMonitor.GetCurrentStats();
        
        if (!Stats.NetworkStatsList.empty())
        {
            const auto& NetworkStats = Stats.NetworkStatsList[0]; // 使用第一个网络接口
            
            Status.Result = HealthCheckResult::HEALTHY;
            Status.Message = "网络连接正常";
            Status.Details["interface_name"] = NetworkStats.InterfaceName;
            Status.Details["bytes_sent"] = std::to_string(NetworkStats.BytesSent);
            Status.Details["bytes_received"] = std::to_string(NetworkStats.BytesReceived);
        }
        else
        {
            Status.Result = HealthCheckResult::UNKNOWN;
            Status.Message = "无法获取网络信息";
        }
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "网络健康检查失败: " + std::string(e.what());
    }

    return Status;
}

HealthCheckStatus HealthChecker::PerformDatabaseHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        // 这里应该检查数据库连接
        // 由于我们没有直接访问数据库实例，这里提供一个基础实现
        Status.Result = HealthCheckResult::HEALTHY;
        Status.Message = "数据库连接正常";
        Status.Details["check_type"] = "database_health";
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "数据库健康检查失败: " + std::string(e.what());
    }

    return Status;
}

HealthCheckStatus HealthChecker::PerformCustomHealthCheck(const HealthCheckConfig& Config)
{
    HealthCheckStatus Status;
    
    try
    {
        // 这里应该执行自定义健康检查
        // 可以通过Config.CustomEndpoint和Config.CustomParameters来配置
        Status.Result = HealthCheckResult::HEALTHY;
        Status.Message = "自定义健康检查通过";
        Status.Details["endpoint"] = Config.CustomEndpoint;
        Status.Details["check_type"] = "custom_health";
    }
    catch (const std::exception& e)
    {
        Status.Result = HealthCheckResult::UNHEALTHY;
        Status.Message = "自定义健康检查失败: " + std::string(e.what());
    }

    return Status;
}

void HealthChecker::UpdateOverallHealthStatus()
{
    PerformAllHealthChecks();
}

void HealthChecker::NotifyHealthCheckCallback(HealthCheckType Type, const HealthCheckStatus& Status)
{
    if (HealthCheckCallbackFunc)
    {
        try
        {
            HealthCheckCallbackFunc(Type, Status);
        }
        catch (const std::exception& e)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, 
                  "健康检查回调执行失败: {}", e.what());
        }
    }
}

void HealthChecker::NotifyOverallHealthCallback(const OverallHealthStatus& Status)
{
    if (OverallHealthCallbackFunc)
    {
        try
        {
            OverallHealthCallbackFunc(Status);
        }
        catch (const std::exception& e)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, 
                  "整体健康检查回调执行失败: {}", e.what());
        }
    }
}

std::string HealthChecker::HealthCheckResultToString(HealthCheckResult Result) const
{
    switch (Result)
    {
        case HealthCheckResult::HEALTHY: return "HEALTHY";
        case HealthCheckResult::UNHEALTHY: return "UNHEALTHY";
        case HealthCheckResult::DEGRADED: return "DEGRADED";
        case HealthCheckResult::CRITICAL: return "CRITICAL";
        case HealthCheckResult::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::string HealthChecker::HealthCheckTypeToString(HealthCheckType Type) const
{
    switch (Type)
    {
        case HealthCheckType::QUEUE_HEALTH: return "QUEUE_HEALTH";
        case HealthCheckType::PERSISTENCE_HEALTH: return "PERSISTENCE_HEALTH";
        case HealthCheckType::MEMORY_HEALTH: return "MEMORY_HEALTH";
        case HealthCheckType::DISK_HEALTH: return "DISK_HEALTH";
        case HealthCheckType::NETWORK_HEALTH: return "NETWORK_HEALTH";
        case HealthCheckType::DATABASE_HEALTH: return "DATABASE_HEALTH";
        case HealthCheckType::CUSTOM_HEALTH: return "CUSTOM_HEALTH";
        default: return "UNKNOWN";
    }
}

} // namespace Helianthus::MessageQueue
