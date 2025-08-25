#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "Discovery/ServiceRegistry.h"
#include "Discovery/ServiceDiscovery.h"
#include "Discovery/HealthChecker.h"
#include "Discovery/LoadBalancer.h"
#include "Discovery/DiscoveryTypes.h"
#include "Shared/Common/StructuredLogger.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Discovery;
using namespace Helianthus::Common;
using namespace Helianthus::Network;

class DiscoveryExample
{
public:
    DiscoveryExample() = default;
    ~DiscoveryExample() = default;

    void RunServer()
    {
        std::cout << "=== 启动服务发现服务器 ===" << std::endl;

        // 初始化日志
        StructuredLoggerConfig logConfig;
        logConfig.MinLevel = StructuredLogLevel::INFO;
        StructuredLogger::Initialize(logConfig);

        // 创建服务注册中心
        Registry_ = std::make_unique<ServiceRegistry>();
        
        // 配置注册中心
        RegistryConfig config;
        config.MaxServices = 1000;
        config.MaxInstancesPerService = 100;
        config.DefaultTtlMs = 300000;  // 5分钟
        config.CleanupIntervalMs = 60000;  // 1分钟
        config.HeartbeatTimeoutMs = 90000;  // 1.5分钟
        config.EnablePersistence = false;
        config.EnableReplication = false;

        // 初始化注册中心
        auto result = Registry_->Initialize(config);
        if (result != DiscoveryResult::SUCCESS)
        {
            std::cerr << "注册中心初始化失败: " << static_cast<int>(result) << std::endl;
            return;
        }

        std::cout << "服务注册中心已启动" << std::endl;

        // 设置回调函数
        Registry_->SetServiceStateChangeCallback(
            [](ServiceInstanceId instanceId, ServiceState oldState, ServiceState newState) {
                std::cout << "服务状态变更: 实例 " << instanceId 
                          << " 从 " << static_cast<int>(oldState) 
                          << " 变为 " << static_cast<int>(newState) << std::endl;
            });

        Registry_->SetServiceRegistrationCallback(
            [](ServiceInstanceId instanceId, DiscoveryResult result) {
                if (result == DiscoveryResult::SUCCESS)
                {
                    std::cout << "服务注册成功: 实例 " << instanceId << std::endl;
                }
                else
                {
                    std::cout << "服务注册失败: 实例 " << instanceId 
                              << " 错误: " << static_cast<int>(result) << std::endl;
                }
            });

        // 注册一些示例服务
        RegisterExampleServices();

        // 启动健康检查器
        StartHealthChecker();

        // 主循环
        std::cout << "服务发现服务器运行中... (按 Ctrl+C 退出)" << std::endl;
        while (Running_)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 定期清理过期服务
            Registry_->CleanupExpiredServices();
            
            // 显示统计信息
            static int counter = 0;
            if (++counter % 30 == 0)  // 每30秒显示一次
            {
                ShowStatistics();
            }
        }

        std::cout << "服务发现服务器已停止" << std::endl;
    }

    void RunClient()
    {
        std::cout << "=== 启动服务发现客户端 ===" << std::endl;

        // 初始化日志
        StructuredLoggerConfig logConfig;
        logConfig.MinLevel = StructuredLogLevel::INFO;
        StructuredLogger::Initialize(logConfig);

        // 创建服务发现客户端
        Discovery_ = std::make_unique<ServiceDiscovery>();

        // 配置服务发现
        RegistryConfig registryConfig;
        registryConfig.MaxServices = 1000;
        registryConfig.DefaultTtlMs = 300000;
        registryConfig.EnablePersistence = false;

        HealthCheckConfig healthConfig;
        healthConfig.Type = HealthCheckType::TCP_CONNECT;
        healthConfig.IntervalMs = 30000;
        healthConfig.TimeoutMs = 5000;
        healthConfig.Enabled = true;

        LoadBalanceConfig loadBalanceConfig;
        loadBalanceConfig.DefaultStrategy = LoadBalanceStrategy::ROUND_ROBIN;
        loadBalanceConfig.EnableHealthAware = true;

        auto result = Discovery_->Initialize(registryConfig, healthConfig, loadBalanceConfig);
        if (result != DiscoveryResult::SUCCESS)
        {
            std::cerr << "服务发现客户端初始化失败: " << static_cast<int>(result) << std::endl;
            return;
        }

        std::cout << "服务发现客户端已启动" << std::endl;

        // 设置回调函数
        Discovery_->SetServiceStateChangeCallback(
            [](ServiceInstanceId instanceId, ServiceState oldState, ServiceState newState) {
                std::cout << "服务状态变更: 实例 " << instanceId 
                          << " 从 " << static_cast<int>(oldState) 
                          << " 变为 " << static_cast<int>(newState) << std::endl;
            });

        Discovery_->SetServiceRegistrationCallback(
            [](ServiceInstanceId instanceId, DiscoveryResult result) {
                if (result == DiscoveryResult::SUCCESS)
                {
                    std::cout << "服务注册成功: 实例 " << instanceId << std::endl;
                }
                else
                {
                    std::cout << "服务注册失败: 实例 " << instanceId 
                              << " 错误: " << static_cast<int>(result) << std::endl;
                }
            });

        // 测试服务发现功能
        TestServiceDiscovery();

        // 测试负载均衡
        TestLoadBalancing();

        // 测试服务监控
        TestServiceMonitoring();

        std::cout << "服务发现客户端测试完成" << std::endl;
    }

private:
    void RegisterExampleServices()
    {
        std::cout << "注册示例服务..." << std::endl;

        // 注册计算器服务实例
        for (int i = 0; i < 3; ++i)
        {
            auto calculatorService = CreateCalculatorService(i);
            ServiceInstanceId instanceId;
            auto result = Registry_->RegisterService(calculatorService, instanceId);
            if (result == DiscoveryResult::SUCCESS)
            {
                std::cout << "注册计算器服务实例 " << i << ": " << instanceId << std::endl;
            }
        }

        // 注册字符串服务实例
        for (int i = 0; i < 2; ++i)
        {
            auto stringService = CreateStringService(i);
            ServiceInstanceId instanceId;
            auto result = Registry_->RegisterService(stringService, instanceId);
            if (result == DiscoveryResult::SUCCESS)
            {
                std::cout << "注册字符串服务实例 " << i << ": " << instanceId << std::endl;
            }
        }

        // 注册数据库服务实例
        for (int i = 0; i < 2; ++i)
        {
            auto dbService = CreateDatabaseService(i);
            ServiceInstanceId instanceId;
            auto result = Registry_->RegisterService(dbService, instanceId);
            if (result == DiscoveryResult::SUCCESS)
            {
                std::cout << "注册数据库服务实例 " << i << ": " << instanceId << std::endl;
            }
        }

        std::cout << "示例服务注册完成" << std::endl;
    }

    ServiceInstance CreateCalculatorService(int index)
    {
        ServiceInstance instance;
        instance.BaseInfo.ServiceName = "CalculatorService";
        instance.BaseInfo.ServiceVersion = "1.0.0";
        instance.BaseInfo.HostAddress = "127.0.0.1";
        instance.BaseInfo.Port = 8081 + index;
        instance.State = ServiceState::HEALTHY;
        instance.CurrentHealthScore = 100;
        instance.Weight = 100;
        instance.Region = "us-west-1";
        instance.Zone = "us-west-1a";
        instance.Environment = "production";
        instance.Tags["service_type"] = "calculator";
        instance.Tags["instance_id"] = std::to_string(index);
        instance.Tags["version"] = "1.0.0";

        // 添加端点
        ServiceEndpoint endpoint;
        endpoint.Address = NetworkAddress("127.0.0.1", 8081 + index);
        endpoint.Protocol = "tcp";
        endpoint.Metadata["rpc_port"] = std::to_string(8081 + index);
        instance.Endpoints.push_back(endpoint);

        return instance;
    }

    ServiceInstance CreateStringService(int index)
    {
        ServiceInstance instance;
        instance.BaseInfo.ServiceName = "StringService";
        instance.BaseInfo.ServiceVersion = "1.0.0";
        instance.BaseInfo.HostAddress = "127.0.0.1";
        instance.BaseInfo.Port = 8091 + index;
        instance.State = ServiceState::HEALTHY;
        instance.CurrentHealthScore = 100;
        instance.Weight = 100;
        instance.Region = "us-west-1";
        instance.Zone = "us-west-1b";
        instance.Environment = "production";
        instance.Tags["service_type"] = "string";
        instance.Tags["instance_id"] = std::to_string(index);
        instance.Tags["version"] = "1.0.0";

        // 添加端点
        ServiceEndpoint endpoint;
        endpoint.Address = NetworkAddress("127.0.0.1", 8091 + index);
        endpoint.Protocol = "tcp";
        endpoint.Metadata["rpc_port"] = std::to_string(8091 + index);
        instance.Endpoints.push_back(endpoint);

        return instance;
    }

    ServiceInstance CreateDatabaseService(int index)
    {
        ServiceInstance instance;
        instance.BaseInfo.ServiceName = "DatabaseService";
        instance.BaseInfo.ServiceVersion = "1.0.0";
        instance.BaseInfo.HostAddress = "127.0.0.1";
        instance.BaseInfo.Port = 8101 + index;
        instance.State = ServiceState::HEALTHY;
        instance.CurrentHealthScore = 100;
        instance.Weight = 150;  // 数据库服务权重更高
        instance.Region = "us-west-1";
        instance.Zone = "us-west-1c";
        instance.Environment = "production";
        instance.Tags["service_type"] = "database";
        instance.Tags["instance_id"] = std::to_string(index);
        instance.Tags["version"] = "1.0.0";
        instance.Tags["db_type"] = "mysql";

        // 添加端点
        ServiceEndpoint endpoint;
        endpoint.Address = NetworkAddress("127.0.0.1", 8101 + index);
        endpoint.Protocol = "tcp";
        endpoint.Metadata["db_port"] = std::to_string(3306 + index);
        instance.Endpoints.push_back(endpoint);

        return instance;
    }

    void StartHealthChecker()
    {
        std::cout << "启动健康检查器..." << std::endl;

        HealthChecker_ = std::make_unique<HealthChecker>();
        
        // 配置健康检查
        HealthCheckConfig config;
        config.Type = HealthCheckType::TCP_CONNECT;
        config.IntervalMs = 30000;  // 30秒
        config.TimeoutMs = 5000;    // 5秒
        config.MaxRetries = 3;
        config.UnhealthyThreshold = 3;
        config.HealthyThreshold = 2;
        config.Enabled = true;

        HealthChecker_->Initialize(config);

        // 设置健康检查回调
        HealthChecker_->SetHealthCheckCallback(
            [this](ServiceInstanceId instanceId, bool isHealthy, HealthScore score) {
                std::cout << "健康检查结果: 实例 " << instanceId 
                          << " 健康: " << (isHealthy ? "是" : "否")
                          << " 分数: " << score << std::endl;
                
                if (Registry_)
                {
                    Registry_->UpdateServiceHealth(instanceId, score);
                    Registry_->UpdateServiceState(instanceId, 
                        isHealthy ? ServiceState::HEALTHY : ServiceState::UNHEALTHY);
                }
            });

        // 启动健康检查线程
        HealthCheckThread_ = std::thread([this]() {
            while (Running_)
            {
                if (Registry_)
                {
                    auto services = Registry_->GetAllServices();
                    for (const auto& service : services)
                    {
                        if (service && !service->Endpoints.empty())
                        {
                            // 注册健康检查
                            HealthCheckConfig healthConfig;
                            healthConfig.Type = HealthCheckType::TCP_CONNECT;
                            healthConfig.IntervalMs = 30000;
                            healthConfig.TimeoutMs = 5000;
                            healthConfig.Enabled = true;
                            
                            HealthChecker_->RegisterHealthCheck(service->InstanceId, healthConfig);
                            HealthChecker_->StartHealthCheck(service->InstanceId);
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }
        });
    }

    void TestServiceDiscovery()
    {
        std::cout << "\n=== 测试服务发现功能 ===" << std::endl;

        // 发现健康服务
        auto calculatorServices = Discovery_->DiscoverHealthyServices("CalculatorService");
        std::cout << "发现健康计算器服务: " << calculatorServices.size() << " 个实例" << std::endl;

        auto stringServices = Discovery_->DiscoverHealthyServices("StringService");
        std::cout << "发现健康字符串服务: " << stringServices.size() << " 个实例" << std::endl;

        auto dbServices = Discovery_->DiscoverHealthyServices("DatabaseService");
        std::cout << "发现健康数据库服务: " << dbServices.size() << " 个实例" << std::endl;

        // 使用负载均衡发现服务
        auto calculatorInstance = Discovery_->DiscoverService("CalculatorService");
        if (calculatorInstance)
        {
            std::cout << "负载均衡选择计算器服务: " << calculatorInstance->InstanceId 
                      << " 地址: " << calculatorInstance->BaseInfo.HostAddress << ":" << calculatorInstance->BaseInfo.Port << std::endl;
        }

        auto dbInstance = Discovery_->DiscoverService("DatabaseService", LoadBalanceStrategy::WEIGHTED_RANDOM);
        if (dbInstance)
        {
            std::cout << "加权随机选择数据库服务: " << dbInstance->InstanceId 
                      << " 权重: " << dbInstance->Weight << std::endl;
        }

        // 获取所有注册的服务
        auto allServices = Discovery_->GetAllRegisteredServices();
        std::cout << "所有注册服务: " << allServices.size() << " 个实例" << std::endl;
        for (const auto& service : allServices)
        {
            std::cout << "  - " << service->BaseInfo.ServiceName << " 实例 " << service->InstanceId 
                      << " 状态: " << static_cast<int>(service->State) << std::endl;
        }
    }

    void TestLoadBalancing()
    {
        std::cout << "\n=== 测试负载均衡功能 ===" << std::endl;

        // 测试轮询负载均衡
        std::cout << "轮询负载均衡测试:" << std::endl;
        for (int i = 0; i < 5; ++i)
        {
            auto instance = Discovery_->DiscoverService("CalculatorService", 
                                                       LoadBalanceStrategy::ROUND_ROBIN);
            if (instance)
            {
                std::cout << "  选择实例 " << instance->InstanceId 
                          << " 端口: " << instance->BaseInfo.Port << std::endl;
            }
        }

        // 测试加权随机负载均衡
        std::cout << "加权随机负载均衡测试:" << std::endl;
        for (int i = 0; i < 5; ++i)
        {
            auto instance = Discovery_->DiscoverService("DatabaseService", 
                                                       LoadBalanceStrategy::WEIGHTED_RANDOM);
            if (instance)
            {
                std::cout << "  选择实例 " << instance->InstanceId 
                          << " 权重: " << instance->Weight << std::endl;
            }
        }

        // 测试最少连接负载均衡
        std::cout << "最少连接负载均衡测试:" << std::endl;
        for (int i = 0; i < 3; ++i)
        {
            auto instance = Discovery_->DiscoverService("StringService", 
                                                       LoadBalanceStrategy::LEAST_CONNECTIONS);
            if (instance)
            {
                std::cout << "  选择实例 " << instance->InstanceId 
                          << " 连接数: " << instance->ActiveConnections << std::endl;
            }
        }
    }

    void TestServiceMonitoring()
    {
        std::cout << "\n=== 测试服务监控功能 ===" << std::endl;

        // 获取服务统计信息
        auto serviceStats = Discovery_->GetServiceStats();
        std::cout << "服务统计信息:" << std::endl;
        for (const auto& [serviceName, count] : serviceStats)
        {
            std::cout << "  - " << serviceName << ": " << count << " 个实例" << std::endl;
        }

        // 获取健康分数
        auto healthScores = Discovery_->GetHealthScores();
        std::cout << "健康分数信息:" << std::endl;
        for (const auto& [instanceId, score] : healthScores)
        {
            std::cout << "  - 实例 " << instanceId << ": " << score << " 分" << std::endl;
        }

        // 获取负载均衡统计
        auto loadBalanceStats = Discovery_->GetLoadBalancingStats();
        std::cout << "负载均衡统计:" << std::endl;
        for (const auto& [serviceName, count] : loadBalanceStats)
        {
            std::cout << "  - " << serviceName << ": " << count << " 次选择" << std::endl;
        }

        // 获取发现信息
        auto discoveryInfo = Discovery_->GetDiscoveryInfo();
        std::cout << "发现系统信息: " << discoveryInfo << std::endl;

        // 等待一段时间观察监控效果
        std::cout << "监控服务状态变化中... (3秒)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    void ShowStatistics()
    {
        if (Registry_)
        {
            auto stats = Registry_->GetRegistryStats();
            std::cout << "\n=== 注册中心统计信息 ===" << std::endl;
            std::cout << "总服务数: " << stats.TotalServices << std::endl;
            std::cout << "健康服务数: " << stats.HealthyServices << std::endl;
            std::cout << "不健康服务数: " << stats.UnhealthyServices << std::endl;
            std::cout << "总实例数: " << stats.TotalServiceInstances << std::endl;
            std::cout << "注册次数: " << stats.RegistrationCount << std::endl;
            std::cout << "注销次数: " << stats.DeregistrationCount << std::endl;
            std::cout << "发现请求数: " << stats.DiscoveryRequestCount << std::endl;
            std::cout << "健康检查数: " << stats.HealthCheckCount << std::endl;
            std::cout << "失败健康检查数: " << stats.FailedHealthCheckCount << std::endl;
        }
    }

    std::unique_ptr<ServiceRegistry> Registry_;
    std::unique_ptr<ServiceDiscovery> Discovery_;
    std::unique_ptr<HealthChecker> HealthChecker_;
    std::thread HealthCheckThread_;
    bool Running_ = true;
};

int main(int argc, char* argv[])
{
    std::cout << "=== Helianthus 服务发现示例程序 ===" << std::endl;
    
    bool runServer = false;
    bool runClient = false;
    
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--server")
        {
            runServer = true;
        }
        else if (arg == "--client")
        {
            runClient = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  --server              运行服务发现服务器" << std::endl;
            std::cout << "  --client              运行服务发现客户端" << std::endl;
            std::cout << "  --help, -h            显示此帮助信息" << std::endl;
            std::cout << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  " << argv[0] << " --server" << std::endl;
            std::cout << "  " << argv[0] << " --client" << std::endl;
            return 0;
        }
    }

    if (!runServer && !runClient)
    {
        std::cout << "请指定 --server 或 --client 参数" << std::endl;
        std::cout << "使用 --help 查看帮助信息" << std::endl;
        return 1;
    }

    DiscoveryExample example;

    if (runServer)
    {
        example.RunServer();
    }
    else if (runClient)
    {
        example.RunClient();
    }

    return 0;
}
