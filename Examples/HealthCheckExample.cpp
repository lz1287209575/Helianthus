#include "Shared/MessageQueue/HealthCheck.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/ResourceMonitor.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace Helianthus::MessageQueue;
using namespace Helianthus::Common;

// 健康检查回调函数
void OnHealthCheck(HealthCheckType Type, const HealthCheckStatus& Status)
{
    std::cout << "🔍 健康检查: " << static_cast<int>(Type) 
              << " = " << static_cast<int>(Status.Result) 
              << " (" << Status.Message << ")" << std::endl;
}

void OnOverallHealth(const OverallHealthStatus& Status)
{
    std::cout << "📊 整体健康状态: " << static_cast<int>(Status.OverallResult) 
              << " - " << Status.OverallMessage << std::endl;
    std::cout << "   健康: " << Status.HealthyChecks 
              << ", 不健康: " << Status.UnhealthyChecks 
              << ", 降级: " << Status.DegradedChecks 
              << ", 严重: " << Status.CriticalChecks << std::endl;
}

int main()
{
    // 初始化日志系统
    Logger::Initialize();
    
    std::cout << "=== Helianthus 健康检查系统演示 ===" << std::endl;
    
    // 初始化资源监控器
    auto& ResourceMonitor = GetResourceMonitor();
    ResourceMonitorConfig ResourceConfig;
    ResourceConfig.SamplingIntervalMs = 2000;
    ResourceConfig.HistoryWindowMs = 60000;
    ResourceConfig.EnableCpuMonitoring = true;
    ResourceConfig.EnableMemoryMonitoring = true;
    ResourceConfig.EnableDiskMonitoring = true;
    ResourceConfig.EnableNetworkMonitoring = true;
    
    ResourceMonitor.Initialize(ResourceConfig);
    ResourceMonitor.StartMonitoring();
    
    // 初始化健康检查器
    auto& HealthChecker = GetHealthChecker();
    HealthChecker.Initialize();
    
    // 设置回调
    HealthChecker.SetHealthCheckCallback(OnHealthCheck);
    HealthChecker.SetOverallHealthCallback(OnOverallHealth);
    
    // 注册各种健康检查
    std::cout << "\n📋 注册健康检查..." << std::endl;
    
    // 队列健康检查
    HealthCheckConfig QueueConfig;
    QueueConfig.Type = HealthCheckType::QUEUE_HEALTH;
    QueueConfig.IntervalMs = 10000;  // 10秒
    QueueConfig.QueueName = "test_queue";
    HealthChecker.RegisterHealthCheck(HealthCheckType::QUEUE_HEALTH, QueueConfig);
    
    // 持久化健康检查
    HealthCheckConfig PersistenceConfig;
    PersistenceConfig.Type = HealthCheckType::PERSISTENCE_HEALTH;
    PersistenceConfig.IntervalMs = 15000;  // 15秒
    HealthChecker.RegisterHealthCheck(HealthCheckType::PERSISTENCE_HEALTH, PersistenceConfig);
    
    // 内存健康检查
    HealthCheckConfig MemoryConfig;
    MemoryConfig.Type = HealthCheckType::MEMORY_HEALTH;
    MemoryConfig.IntervalMs = 8000;  // 8秒
    HealthChecker.RegisterHealthCheck(HealthCheckType::MEMORY_HEALTH, MemoryConfig);
    
    // 磁盘健康检查
    HealthCheckConfig DiskConfig;
    DiskConfig.Type = HealthCheckType::DISK_HEALTH;
    DiskConfig.IntervalMs = 20000;  // 20秒
    HealthChecker.RegisterHealthCheck(HealthCheckType::DISK_HEALTH, DiskConfig);
    
    // 网络健康检查
    HealthCheckConfig NetworkConfig;
    NetworkConfig.Type = HealthCheckType::NETWORK_HEALTH;
    NetworkConfig.IntervalMs = 12000;  // 12秒
    HealthChecker.RegisterHealthCheck(HealthCheckType::NETWORK_HEALTH, NetworkConfig);
    
    // 数据库健康检查
    HealthCheckConfig DatabaseConfig;
    DatabaseConfig.Type = HealthCheckType::DATABASE_HEALTH;
    DatabaseConfig.IntervalMs = 25000;  // 25秒
    HealthChecker.RegisterHealthCheck(HealthCheckType::DATABASE_HEALTH, DatabaseConfig);
    
    // 自定义健康检查
    HealthCheckConfig CustomConfig;
    CustomConfig.Type = HealthCheckType::CUSTOM_HEALTH;
    CustomConfig.IntervalMs = 30000;  // 30秒
    CustomConfig.CustomEndpoint = "http://localhost:8080/health";
    HealthChecker.RegisterHealthCheck(HealthCheckType::CUSTOM_HEALTH, CustomConfig);
    
    // 启动健康检查
    std::cout << "\n🚀 启动健康检查..." << std::endl;
    HealthChecker.StartHealthChecks();
    
    // 演示手动健康检查
    std::cout << "\n🔧 执行手动健康检查..." << std::endl;
    
    for (int i = 0; i < 7; ++i)
    {
        HealthCheckType Type = static_cast<HealthCheckType>(i);
        auto Status = HealthChecker.PerformHealthCheck(Type);
        
        std::cout << "手动检查 " << static_cast<int>(Type) << ": " 
                  << static_cast<int>(Status.Result) << " - " 
                  << Status.Message << " (响应时间: " << Status.ResponseTimeMs << "ms)" << std::endl;
    }
    
    // 执行所有健康检查
    std::cout << "\n📊 执行所有健康检查..." << std::endl;
    auto OverallStatus = HealthChecker.PerformAllHealthChecks();
    
    std::cout << "整体状态: " << static_cast<int>(OverallStatus.OverallResult) 
              << " - " << OverallStatus.OverallMessage << std::endl;
    
    // 显示详细状态
    std::cout << "\n📋 详细健康状态:" << std::endl;
    for (const auto& Pair : OverallStatus.CheckStatuses)
    {
        const auto& Status = Pair.second;
        std::cout << "  " << static_cast<int>(Pair.first) << ": " 
                  << static_cast<int>(Status.Result) << " - " 
                  << Status.Message << " (成功率: " << (Status.SuccessRate * 100) << "%)" << std::endl;
    }
    
    // 显示问题和警告
    if (!OverallStatus.Issues.empty())
    {
        std::cout << "\n⚠️ 问题:" << std::endl;
        for (const auto& Issue : OverallStatus.Issues)
        {
            std::cout << "  - " << Issue << std::endl;
        }
    }
    
    if (!OverallStatus.Warnings.empty())
    {
        std::cout << "\n⚠️ 警告:" << std::endl;
        for (const auto& Warning : OverallStatus.Warnings)
        {
            std::cout << "  - " << Warning << std::endl;
        }
    }
    
    // 运行一段时间观察自动健康检查
    std::cout << "\n⏰ 运行30秒观察自动健康检查..." << std::endl;
    for (int i = 0; i < 30; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (i % 10 == 0)
        {
            auto CurrentStatus = HealthChecker.GetOverallHealthStatus();
            std::cout << "第" << (i + 1) << "秒 - 整体状态: " 
                      << static_cast<int>(CurrentStatus.OverallResult) 
                      << " (" << CurrentStatus.OverallMessage << ")" << std::endl;
        }
    }
    
    // 显示统计信息
    std::cout << "\n📈 健康检查统计:" << std::endl;
    for (int i = 0; i < 7; ++i)
    {
        HealthCheckType Type = static_cast<HealthCheckType>(i);
        auto Status = HealthChecker.GetHealthStatus(Type);
        
        std::cout << "  " << static_cast<int>(Type) << ": "
                  << "总检查: " << Status.TotalChecks
                  << ", 失败: " << Status.TotalFailures
                  << ", 成功率: " << (Status.SuccessRate * 100) << "%"
                  << ", 平均响应时间: " << Status.ResponseTimeMs << "ms" << std::endl;
    }
    
    // 停止健康检查
    std::cout << "\n🛑 停止健康检查..." << std::endl;
    HealthChecker.StopHealthChecks();
    
    // 清理
    ResourceMonitor.StopMonitoring();
    
    std::cout << "\n=== 健康检查系统演示完成 ===" << std::endl;
    
    return 0;
}
