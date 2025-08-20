#pragma once

#include <functional>
#include <memory>
#include <string>

#include "../RPC/IRpcServer.h"
#include "HealthChecker.h"
#include "IHealthChecker.h"
#include "ILoadBalancer.h"
#include "IServiceRegistry.h"
#include "LoadBalancer.h"
#include "ServiceRegistry.h"

namespace Helianthus::Discovery
{
/**
 * @brief 服务发现主控制器
 *
 * 整合服务注册、健康检查和负载均衡功能，
 * 为RPC框架和其他客户端提供统一的服务发现接口
 */
class ServiceDiscovery
{
public:
    ServiceDiscovery();
    ~ServiceDiscovery();

    // 禁用拷贝构造和赋值
    ServiceDiscovery(const ServiceDiscovery&) = delete;
    ServiceDiscovery& operator=(const ServiceDiscovery&) = delete;

    // 移动构造和赋值
    ServiceDiscovery(ServiceDiscovery&& Other) noexcept;
    ServiceDiscovery& operator=(ServiceDiscovery&& Other) noexcept;

    // 初始化和生命周期
    DiscoveryResult Initialize(const RegistryConfig& RegistryConfig,
                               const HealthCheckConfig& HealthConfig,
                               const LoadBalanceConfig& LoadBalanceConfig);
    void Shutdown();
    bool IsInitialized() const;

    // 组件访问
    IServiceRegistry* GetServiceRegistry() const;
    IHealthChecker* GetHealthChecker() const;
    ILoadBalancer* GetLoadBalancer() const;

    // 高级服务注册 - 自动集成健康检查和负载均衡
    DiscoveryResult RegisterServiceWithRpc(std::shared_ptr<RPC::IRpcServer> RpcServer,
                                           const Common::ServiceInfo& ServiceInfo,
                                           const HealthCheckConfig& HealthConfig,
                                           ServiceInstanceId& OutInstanceId);

    DiscoveryResult RegisterService(const ServiceInstance& Instance,
                                    const HealthCheckConfig& HealthConfig,
                                    ServiceInstanceId& OutInstanceId);

    DiscoveryResult DeregisterService(ServiceInstanceId InstanceId);

    // 高级服务发现 - 结合负载均衡和健康检查
    ServiceInstancePtr
    DiscoverService(const std::string& ServiceName,
                    LoadBalanceStrategy Strategy = LoadBalanceStrategy::ROUND_ROBIN);

    ServiceInstancePtr DiscoverServiceWithContext(const std::string& ServiceName,
                                                  const std::string& Context);

    std::vector<ServiceInstancePtr> DiscoverHealthyServices(const std::string& ServiceName);

    // RPC集成助手
    DiscoveryResult ConnectToService(const std::string& ServiceName,
                                     std::function<void(ServiceInstancePtr)> OnConnected,
                                     std::function<void(const std::string&)> OnError = nullptr);

    DiscoveryResult CallService(const std::string& ServiceName,
                                const std::string& MethodName,
                                const std::string& RequestData,
                                std::function<void(const std::string&)> OnResponse,
                                std::function<void(const std::string&)> OnError = nullptr);

    // 服务监控和统计
    std::vector<ServiceInstancePtr> GetAllRegisteredServices() const;
    std::unordered_map<std::string, uint32_t> GetServiceStats() const;
    std::unordered_map<ServiceInstanceId, HealthScore> GetHealthScores() const;
    std::unordered_map<std::string, uint64_t> GetLoadBalancingStats() const;

    // 配置管理
    void UpdateRegistryConfig(const RegistryConfig& Config);
    void UpdateHealthCheckConfig(const HealthCheckConfig& Config);
    void UpdateLoadBalanceConfig(const LoadBalanceConfig& Config);

    // 事件回调
    void SetServiceStateChangeCallback(
        std::function<void(ServiceInstanceId, ServiceState, ServiceState)> Callback);
    void SetServiceRegistrationCallback(
        std::function<void(ServiceInstanceId, DiscoveryResult)> Callback);
    void SetHealthAlertCallback(
        std::function<void(ServiceInstanceId, HealthScore, const std::string&)> Callback);
    void
    SetLoadBalanceCallback(std::function<void(ServiceInstanceId, const std::string&)> Callback);

    // 维护模式
    void SetMaintenanceMode(bool Enable);
    bool IsInMaintenanceMode() const;

    // 集群和复制
    DiscoveryResult EnableReplication(const std::vector<Network::NetworkAddress>& ReplicaNodes);
    void DisableReplication();
    bool IsReplicationEnabled() const;

    // 调试和诊断
    std::string GetDiscoveryInfo() const;
    DiscoveryResult ValidateConfiguration() const;

private:
    // 内部组件同步
    void SyncRegistryWithLoadBalancer();
    void SyncHealthWithLoadBalancer();
    void StartPeriodicSync();
    void StopPeriodicSync();
    void PeriodicSyncLoop();

    // 事件处理
    void OnServiceStateChanged(ServiceInstanceId InstanceId,
                               ServiceState OldState,
                               ServiceState NewState);
    void OnServiceRegistered(ServiceInstanceId InstanceId, DiscoveryResult Result);
    void OnHealthAlert(ServiceInstanceId InstanceId, HealthScore Score, const std::string& Message);
    void OnLoadBalanceEvent(ServiceInstanceId InstanceId, const std::string& ServiceName);

    // RPC助手方法
    ServiceInstancePtr SelectBestServiceInstance(const std::string& ServiceName);
    DiscoveryResult CreateRpcConnection(ServiceInstancePtr Instance,
                                        std::function<void(ServiceInstancePtr)> OnConnected,
                                        std::function<void(const std::string&)> OnError);

private:
    // 核心组件
    std::unique_ptr<ServiceRegistry> Registry;
    std::unique_ptr<HealthChecker> HealthCheck;
    std::unique_ptr<LoadBalancer> LoadBalance;

    // 状态
    std::atomic<bool> InitializedFlag{false};
    std::atomic<bool> ShuttingDownFlag{false};
    std::atomic<bool> MaintenanceModeFlag{false};

    // 配置
    RegistryConfig CurrentRegistryConfig;
    HealthCheckConfig CurrentHealthConfig;
    LoadBalanceConfig CurrentLoadBalanceConfig;

    // 同步线程
    std::thread SyncThread;
    std::atomic<bool> StopSyncThread{false};
    std::mutex SyncMutex;

    // 事件回调
    std::function<void(ServiceInstanceId, ServiceState, ServiceState)> ServiceStateChangeCallback;
    std::function<void(ServiceInstanceId, DiscoveryResult)> ServiceRegistrationCallback;
    std::function<void(ServiceInstanceId, HealthScore, const std::string&)> HealthAlertCallbackFunc;
    std::function<void(ServiceInstanceId, const std::string&)> LoadBalanceCallbackFunc;

    // RPC服务器引用（用于自动注册）
    std::unordered_map<ServiceInstanceId, std::weak_ptr<RPC::IRpcServer>> RegisteredRpcServers;
    std::mutex RpcServersMutex;
};

// 全局服务发现实例（单例模式）
class GlobalServiceDiscovery
{
public:
    static ServiceDiscovery& GetInstance();
    static bool Initialize(const RegistryConfig& RegistryConfig,
                           const HealthCheckConfig& HealthConfig,
                           const LoadBalanceConfig& LoadBalanceConfig);
    static void Shutdown();

private:
    static std::unique_ptr<ServiceDiscovery> Instance;
    static std::mutex InstanceMutex;
};

}  // namespace Helianthus::Discovery