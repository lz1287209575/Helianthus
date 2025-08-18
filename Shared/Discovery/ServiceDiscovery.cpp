#include "ServiceDiscovery.h"
#include "IServiceRegistry.h"
#include "ServiceRegistry.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace Helianthus::Discovery
{
    ServiceDiscovery::ServiceDiscovery()
        : Registry(std::make_unique<ServiceRegistry>())
        , HealthCheck(std::make_unique<HealthChecker>())
        , LoadBalance(std::make_unique<LoadBalancer>())
    {
        std::cout << "[ServiceDiscovery] Create Service Discovery Controller" << std::endl;
    }

    ServiceDiscovery::~ServiceDiscovery()
    {
        Shutdown();
    }

    ServiceDiscovery::ServiceDiscovery(ServiceDiscovery&& Other) noexcept
        : Registry(std::move(Other.Registry))
        , HealthCheck(std::move(Other.HealthCheck))
        , LoadBalance(std::move(Other.LoadBalance))
        , InitializedFlag(Other.InitializedFlag.load())
        , ShuttingDownFlag(Other.ShuttingDownFlag.load())
        , MaintenanceModeFlag(Other.MaintenanceModeFlag.load())
        , CurrentRegistryConfig(std::move(Other.CurrentRegistryConfig))
        , CurrentHealthConfig(std::move(Other.CurrentHealthConfig))
        , CurrentLoadBalanceConfig(std::move(Other.CurrentLoadBalanceConfig))
        , StopSyncThread(Other.StopSyncThread.load())
        , ServiceStateChangeCallback(std::move(Other.ServiceStateChangeCallback))
        , ServiceRegistrationCallback(std::move(Other.ServiceRegistrationCallback))
        , HealthAlertCallbackFunc(std::move(Other.HealthAlertCallbackFunc))
        , LoadBalanceCallbackFunc(std::move(Other.LoadBalanceCallbackFunc))
        , RegisteredRpcServers(std::move(Other.RegisteredRpcServers))
    {
        Other.InitializedFlag = false;
        Other.ShuttingDownFlag = false;
    }

    ServiceDiscovery& ServiceDiscovery::operator=(ServiceDiscovery&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown();
            
            Registry = std::move(Other.Registry);
            HealthCheck = std::move(Other.HealthCheck);
            LoadBalance = std::move(Other.LoadBalance);
            InitializedFlag = Other.InitializedFlag.load();
            ShuttingDownFlag = Other.ShuttingDownFlag.load();
            MaintenanceModeFlag = Other.MaintenanceModeFlag.load();
            CurrentRegistryConfig = std::move(Other.CurrentRegistryConfig);
            CurrentHealthConfig = std::move(Other.CurrentHealthConfig);
            CurrentLoadBalanceConfig = std::move(Other.CurrentLoadBalanceConfig);
            StopSyncThread = Other.StopSyncThread.load();
            ServiceStateChangeCallback = std::move(Other.ServiceStateChangeCallback);
            ServiceRegistrationCallback = std::move(Other.ServiceRegistrationCallback);
            HealthAlertCallbackFunc = std::move(Other.HealthAlertCallbackFunc);
            LoadBalanceCallbackFunc = std::move(Other.LoadBalanceCallbackFunc);
            RegisteredRpcServers = std::move(Other.RegisteredRpcServers);
            
            Other.InitializedFlag = false;
            Other.ShuttingDownFlag = false;
        }
        return *this;
    }

    DiscoveryResult ServiceDiscovery::Initialize(const RegistryConfig& RegistryConfig,
                                               const HealthCheckConfig& HealthConfig,
                                               const LoadBalanceConfig& LoadBalanceConfig)
    {
        if (InitializedFlag)
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        std::cout << "[ServiceDiscovery] Start Initialize" << std::endl;

        // 初始化服务注册中心
        auto RegistryResult = Registry->Initialize(RegistryConfig);
        if (RegistryResult != DiscoveryResult::SUCCESS)
        {
            std::cout << "[ServiceDiscovery] Service Registry Central Initialize Failed" << std::endl;
            return RegistryResult;
        }

        // 初始化健康检查器
        auto HealthResult = HealthCheck->Initialize(HealthConfig);
        if (HealthResult != DiscoveryResult::SUCCESS)
        {
            std::cout << "[ServiceDiscovery] Health Checker Initialize Failed" << std::endl;
            Registry->Shutdown();
            return HealthResult;
        }

        // 初始化负载均衡器
        auto LoadBalanceResult = LoadBalance->Initialize(LoadBalanceConfig);
        if (LoadBalanceResult != DiscoveryResult::SUCCESS)
        {
            std::cout << "[ServiceDiscovery] LoadBalance Initialize Failed" << std::endl;
            HealthCheck->Shutdown();
            Registry->Shutdown();
            return LoadBalanceResult;
        }

        // 保存配置
        CurrentRegistryConfig = RegistryConfig;
        CurrentHealthConfig = HealthConfig;
        CurrentLoadBalanceConfig = LoadBalanceConfig;

        // 设置组件间的回调
        Registry->SetServiceStateChangeCallback(
            [this](ServiceInstanceId Id, ServiceState Old, ServiceState New) {
                OnServiceStateChanged(Id, Old, New);
            });

        Registry->SetServiceRegistrationCallback(
            [this](ServiceInstanceId Id, DiscoveryResult Result) {
                OnServiceRegistered(Id, Result);
            });

        HealthCheck->SetHealthStateChangeCallback(
            [this](ServiceInstanceId Id, ServiceState Old, ServiceState New) {
                OnServiceStateChanged(Id, Old, New);
            });

        HealthCheck->SetHealthAlertCallback(
            [this](ServiceInstanceId Id, HealthScore Score, const std::string& Message) {
                OnHealthAlert(Id, Score, Message);
            });

        LoadBalance->SetLoadBalanceCallback(
            [this](ServiceInstanceId Id, const std::string& ServiceName) {
                OnLoadBalanceEvent(Id, ServiceName);
            });

        // 启动同步线程
        StartPeriodicSync();

        InitializedFlag = true;
        ShuttingDownFlag = false;

        std::cout << "[ServiceDiscovery] Initialized Finish" << std::endl;
        return DiscoveryResult::SUCCESS;
    }

    void ServiceDiscovery::Shutdown()
    {
        if (!InitializedFlag)
        {
            return;
        }

        std::cout << "[ServiceDiscovery] Start Shutdown" << std::endl;

        ShuttingDownFlag = true;
        StopPeriodicSync();

        // 清理RPC服务器引用
        {
            std::lock_guard<std::mutex> Lock(RpcServersMutex);
            RegisteredRpcServers.clear();
        }

        // 关闭组件
        if (LoadBalance)
        {
            LoadBalance->Shutdown();
        }

        if (HealthCheck)
        {
            HealthCheck->Shutdown();
        }

        if (Registry)
        {
            Registry->Shutdown();
        }

        InitializedFlag = false;
        std::cout << "[ServiceDiscovery] 关闭完成" << std::endl;
    }

    bool ServiceDiscovery::IsInitialized() const
    {
        return InitializedFlag;
    }

    IServiceRegistry* ServiceDiscovery::GetServiceRegistry() const
    {
        return Registry.get();
    }

    IHealthChecker* ServiceDiscovery::GetHealthChecker() const
    {
        return HealthCheck.get();
    }

    ILoadBalancer* ServiceDiscovery::GetLoadBalancer() const
    {
        return LoadBalance.get();
    }

    DiscoveryResult ServiceDiscovery::RegisterServiceWithRpc(std::shared_ptr<RPC::IRpcServer> RpcServer,
                                                           const Common::ServiceInfo& ServiceInfo,
                                                           const HealthCheckConfig& HealthConfig,
                                                           ServiceInstanceId& OutInstanceId)
    {
        if (!InitializedFlag || !RpcServer)
        {
            return DiscoveryResult::INVALID_SERVICE_INFO;
        }

        // 创建服务实例
        ServiceInstance Instance;
        Instance.BaseInfo = ServiceInfo;
        Instance.State = ServiceState::HEALTHY;
        Instance.CurrentHealthScore = MaxHealthScore;
        Instance.RegisteredTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 从RPC服务器获取网络信息（简化实现）
        // TODO: 实际实现需要从RPC服务器获取真实的地址和端口
        Discovery::ServiceEndpoint Endpoint;
        Endpoint.Address = Network::NetworkAddress("127.0.0.1", 8080); // 默认地址
        Endpoint.Protocol = "RPC";
        Instance.Endpoints.push_back(Endpoint);

        // 注册服务
        auto Result = RegisterService(Instance, HealthConfig, OutInstanceId);
        if (Result == DiscoveryResult::SUCCESS)
        {
            // 保存RPC服务器引用
            std::lock_guard<std::mutex> Lock(RpcServersMutex);
            RegisteredRpcServers[OutInstanceId] = RpcServer;

            std::cout << "[ServiceDiscovery] RPC服务注册成功: " << ServiceInfo.ServiceName 
                      << ", 实例ID: " << OutInstanceId << std::endl;
        }

        return Result;
    }

    DiscoveryResult ServiceDiscovery::RegisterService(const ServiceInstance& Instance,
                                                    const HealthCheckConfig& HealthConfig,
                                                    ServiceInstanceId& OutInstanceId)
    {
        if (!InitializedFlag)
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        // 注册到服务注册中心
        auto RegistryResult = Registry->RegisterService(Instance, OutInstanceId);
        if (RegistryResult != DiscoveryResult::SUCCESS)
        {
            return RegistryResult;
        }

        // 注册健康检查
        auto HealthResult = HealthCheck->RegisterHealthCheck(OutInstanceId, HealthConfig);
        if (HealthResult != DiscoveryResult::SUCCESS)
        {
            Registry->DeregisterService(OutInstanceId);
            return HealthResult;
        }

        // 添加到负载均衡器
        auto ServicePtr = Registry->GetService(OutInstanceId);
        if (ServicePtr)
        {
            auto LoadBalanceResult = LoadBalance->AddServiceInstance(ServicePtr);
            if (LoadBalanceResult != DiscoveryResult::SUCCESS)
            {
                HealthCheck->UnregisterHealthCheck(OutInstanceId);
                Registry->DeregisterService(OutInstanceId);
                return LoadBalanceResult;
            }
        }

        // 启动健康检查
        HealthCheck->StartHealthCheck(OutInstanceId);

        std::cout << "[ServiceDiscovery] 服务注册成功: " << Instance.BaseInfo.ServiceName 
                  << ", 实例ID: " << OutInstanceId << std::endl;

        return DiscoveryResult::SUCCESS;
    }

    DiscoveryResult ServiceDiscovery::DeregisterService(ServiceInstanceId InstanceId)
    {
        if (!InitializedFlag)
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        // 停止健康检查
        HealthCheck->StopHealthCheck(InstanceId);
        HealthCheck->UnregisterHealthCheck(InstanceId);

        // 从负载均衡器移除
        LoadBalance->RemoveServiceInstance(InstanceId);

        // 从服务注册中心移除
        auto Result = Registry->DeregisterService(InstanceId);

        // 清理RPC服务器引用
        {
            std::lock_guard<std::mutex> Lock(RpcServersMutex);
            RegisteredRpcServers.erase(InstanceId);
        }

        std::cout << "[ServiceDiscovery] 服务注销成功，实例ID: " << InstanceId << std::endl;
        return Result;
    }

    ServiceInstancePtr ServiceDiscovery::DiscoverService(const std::string& ServiceName,
                                                       LoadBalanceStrategy Strategy)
    {
        if (!InitializedFlag)
        {
            return nullptr;
        }

        return LoadBalance->SelectInstanceWithStrategy(ServiceName, Strategy);
    }

    ServiceInstancePtr ServiceDiscovery::DiscoverServiceWithContext(const std::string& ServiceName,
                                                                  const std::string& Context)
    {
        if (!InitializedFlag)
        {
            return nullptr;
        }

        return LoadBalance->SelectInstanceWithContext(ServiceName, Context);
    }

    std::vector<ServiceInstancePtr> ServiceDiscovery::DiscoverHealthyServices(const std::string& ServiceName)
    {
        if (!InitializedFlag)
        {
            return {};
        }

        return LoadBalance->GetHealthyInstances(ServiceName);
    }

    DiscoveryResult ServiceDiscovery::ConnectToService(const std::string& ServiceName,
                                                     std::function<void(ServiceInstancePtr)> OnConnected,
                                                     std::function<void(const std::string&)> OnError)
    {
        if (!InitializedFlag)
        {
            if (OnError) OnError("ServiceDiscovery not initialized");
            return DiscoveryResult::INTERNAL_ERROR;
        }

        auto Instance = SelectBestServiceInstance(ServiceName);
        if (!Instance)
        {
            if (OnError) OnError("No available service instance found");
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        return CreateRpcConnection(Instance, OnConnected, OnError);
    }

    DiscoveryResult ServiceDiscovery::CallService(const std::string& ServiceName,
                                                const std::string& MethodName,
                                                const std::string& RequestData,
                                                std::function<void(const std::string&)> OnResponse,
                                                std::function<void(const std::string&)> OnError)
    {
        return ConnectToService(ServiceName,
            [=](ServiceInstancePtr Instance) {
                // 这里应该使用RPC客户端进行实际调用
                // 简化实现：直接调用成功回调
                if (OnResponse)
                {
                    OnResponse("Service call completed for " + MethodName);
                }
            },
            OnError
        );
    }

    std::vector<ServiceInstancePtr> ServiceDiscovery::GetAllRegisteredServices() const
    {
        if (!InitializedFlag)
        {
            return {};
        }

        return Registry->GetAllServices();
    }

    std::unordered_map<std::string, uint32_t> ServiceDiscovery::GetServiceStats() const
    {
        std::unordered_map<std::string, uint32_t> Stats;
        
        if (InitializedFlag)
        {
            std::vector<std::string> ServiceNames = Registry->GetServiceNames();
            for (const std::string& Name : ServiceNames)
            {
                Stats[Name] = LoadBalance->GetServiceInstanceCount(Name);
            }
        }
        
        return Stats;
    }

    std::unordered_map<ServiceInstanceId, HealthScore> ServiceDiscovery::GetHealthScores() const
    {
        if (!InitializedFlag)
        {
            return {};
        }

        return HealthCheck->GetAllHealthScores();
    }

    std::unordered_map<std::string, uint64_t> ServiceDiscovery::GetLoadBalancingStats() const
    {
        if (!InitializedFlag)
        {
            return {};
        }

        return LoadBalance->GetSelectionStats();
    }

    // 私有方法实现
    void ServiceDiscovery::SyncRegistryWithLoadBalancer()
    {
        if (!InitializedFlag || ShuttingDownFlag)
        {
            return;
        }

        std::vector<ServiceInstancePtr> AllServices = Registry->GetAllServices();
        for (const ServiceInstancePtr& Service : AllServices)
        {
            // 确保服务在负载均衡器中
            auto Instances = LoadBalance->GetServiceInstances(Service->BaseInfo.ServiceName);
            bool Found = false;
            
            for (const auto& Instance : Instances)
            {
                if (Instance->InstanceId == Service->InstanceId)
                {
                    Found = true;
                    break;
                }
            }
            
            if (!Found)
            {
                LoadBalance->AddServiceInstance(Service);
            }
        }
    }

    void ServiceDiscovery::SyncHealthWithLoadBalancer()
    {
        if (!InitializedFlag || ShuttingDownFlag)
        {
            return;
        }

        auto HealthScores = HealthCheck->GetAllHealthScores();
        for (const auto& Score : HealthScores)
        {
            LoadBalance->UpdateInstanceHealth(Score.first, Score.second);
        }
    }

    void ServiceDiscovery::StartPeriodicSync()
    {
        StopSyncThread = false;
        SyncThread = std::thread(&ServiceDiscovery::PeriodicSyncLoop, this);
    }

    void ServiceDiscovery::StopPeriodicSync()
    {
        StopSyncThread = true;
        if (SyncThread.joinable())
        {
            SyncThread.join();
        }
    }

    void ServiceDiscovery::PeriodicSyncLoop()
    {
        while (!StopSyncThread && !ShuttingDownFlag)
        {
            {
                std::lock_guard<std::mutex> Lock(SyncMutex);
                SyncRegistryWithLoadBalancer();
                SyncHealthWithLoadBalancer();
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒同步一次
        }
    }

    void ServiceDiscovery::OnServiceStateChanged(ServiceInstanceId InstanceId, ServiceState OldState, ServiceState NewState)
    {
        // 更新负载均衡器中的实例状态
        ServiceInstancePtr Instance = Registry->GetService(InstanceId);
        if (Instance)
        {
            LoadBalance->UpdateServiceInstance(Instance);
        }

        // 调用用户回调
        if (ServiceStateChangeCallback)
        {
            ServiceStateChangeCallback(InstanceId, OldState, NewState);
        }
    }

    void ServiceDiscovery::OnServiceRegistered(ServiceInstanceId InstanceId, DiscoveryResult Result)
    {
        if (ServiceRegistrationCallback)
        {
            ServiceRegistrationCallback(InstanceId, Result);
        }
    }

    void ServiceDiscovery::OnHealthAlert(ServiceInstanceId InstanceId, HealthScore Score, const std::string& Message)
    {
        if (HealthAlertCallbackFunc)
        {
            HealthAlertCallbackFunc(InstanceId, Score, Message);
        }
    }

    void ServiceDiscovery::OnLoadBalanceEvent(ServiceInstanceId InstanceId, const std::string& ServiceName)
    {
        // 记录连接统计
        LoadBalance->RecordConnection(InstanceId);

        if (LoadBalanceCallbackFunc)
        {
            LoadBalanceCallbackFunc(InstanceId, ServiceName);
        }
    }

    ServiceInstancePtr ServiceDiscovery::SelectBestServiceInstance(const std::string& ServiceName)
    {
        return LoadBalance->SelectInstance(ServiceName);
    }

    DiscoveryResult ServiceDiscovery::CreateRpcConnection(ServiceInstancePtr Instance,
                                                        std::function<void(ServiceInstancePtr)> OnConnected,
                                                        std::function<void(const std::string&)> OnError)
    {
        if (!Instance)
        {
            if (OnError) OnError("Invalid service instance");
            return DiscoveryResult::INVALID_SERVICE_INFO;
        }

        // 简化实现：直接调用成功回调
        if (OnConnected)
        {
            OnConnected(Instance);
        }

        return DiscoveryResult::SUCCESS;
    }

    // 配置管理和回调方法
    void ServiceDiscovery::UpdateRegistryConfig(const RegistryConfig& Config)
    {
        if (InitializedFlag && Registry)
        {
            Registry->UpdateConfig(Config);
            CurrentRegistryConfig = Config;
        }
    }

    void ServiceDiscovery::UpdateHealthCheckConfig(const HealthCheckConfig& Config)
    {
        if (InitializedFlag && HealthCheck)
        {
            HealthCheck->UpdateDefaultConfig(Config);
            CurrentHealthConfig = Config;
        }
    }

    void ServiceDiscovery::UpdateLoadBalanceConfig(const LoadBalanceConfig& Config)
    {
        if (InitializedFlag && LoadBalance)
        {
            LoadBalance->UpdateConfig(Config);
            CurrentLoadBalanceConfig = Config;
        }
    }

    void ServiceDiscovery::SetServiceStateChangeCallback(std::function<void(ServiceInstanceId, ServiceState, ServiceState)> Callback)
    {
        ServiceStateChangeCallback = std::move(Callback);
    }

    void ServiceDiscovery::SetServiceRegistrationCallback(std::function<void(ServiceInstanceId, DiscoveryResult)> Callback)
    {
        ServiceRegistrationCallback = std::move(Callback);
    }

    void ServiceDiscovery::SetHealthAlertCallback(std::function<void(ServiceInstanceId, HealthScore, const std::string&)> Callback)
    {
        HealthAlertCallbackFunc = std::move(Callback);
    }

    void ServiceDiscovery::SetLoadBalanceCallback(std::function<void(ServiceInstanceId, const std::string&)> Callback)
    {
        LoadBalanceCallbackFunc = std::move(Callback);
    }

    void ServiceDiscovery::SetMaintenanceMode(bool Enable)
    {
        MaintenanceModeFlag = Enable;
        if (Registry)
        {
            Registry->SetMaintenanceMode(Enable);
        }
    }

    bool ServiceDiscovery::IsInMaintenanceMode() const
    {
        return MaintenanceModeFlag;
    }

    DiscoveryResult ServiceDiscovery::EnableReplication(const std::vector<Network::NetworkAddress>& ReplicaNodes)
    {
        if (Registry)
        {
            return Registry->EnableReplication(ReplicaNodes);
        }
        return DiscoveryResult::INTERNAL_ERROR;
    }

    void ServiceDiscovery::DisableReplication()
    {
        if (Registry)
        {
            Registry->DisableReplication();
        }
    }

    bool ServiceDiscovery::IsReplicationEnabled() const
    {
        return Registry ? Registry->IsReplicationEnabled() : false;
    }

    std::string ServiceDiscovery::GetDiscoveryInfo() const
    {
        std::stringstream Info;
        Info << "ServiceDiscovery Status:\n";
        Info << "  Initialized: " << (InitializedFlag ? "Yes" : "No") << "\n";
        Info << "  Maintenance Mode: " << (MaintenanceModeFlag ? "Yes" : "No") << "\n";
        
        if (Registry)
        {
            Info << "  Total Services: " << Registry->GetServiceCount() << "\n";
            Info << "  Total Instances: " << Registry->GetServiceInstanceCount() << "\n";
        }
        
        return Info.str();
    }

    DiscoveryResult ServiceDiscovery::ValidateConfiguration() const
    {
        if (!InitializedFlag)
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        // 验证组件状态
        if (!Registry || !Registry->IsInitialized())
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        if (!HealthCheck || !HealthCheck->IsInitialized())
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        if (!LoadBalance || !LoadBalance->IsInitialized())
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        return DiscoveryResult::SUCCESS;
    }

    // 全局服务发现实例实现
    std::unique_ptr<ServiceDiscovery> GlobalServiceDiscovery::Instance = nullptr;
    std::mutex GlobalServiceDiscovery::InstanceMutex;

    ServiceDiscovery& GlobalServiceDiscovery::GetInstance()
    {
        std::lock_guard<std::mutex> Lock(InstanceMutex);
        if (!Instance)
        {
            Instance = std::make_unique<ServiceDiscovery>();
        }
        return *Instance;
    }

    bool GlobalServiceDiscovery::Initialize(const RegistryConfig& RegistryConfig,
                                          const HealthCheckConfig& HealthConfig,
                                          const LoadBalanceConfig& LoadBalanceConfig)
    {
        auto& Discovery = GetInstance();
        return Discovery.Initialize(RegistryConfig, HealthConfig, LoadBalanceConfig) == DiscoveryResult::SUCCESS;
    }

    void GlobalServiceDiscovery::Shutdown()
    {
        std::lock_guard<std::mutex> Lock(InstanceMutex);
        if (Instance)
        {
            Instance->Shutdown();
            Instance.reset();
        }
    }

} // namespace Helianthus::Discovery