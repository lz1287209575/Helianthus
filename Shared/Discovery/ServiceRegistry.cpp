#include "Discovery/ServiceRegistry.h"
#include <algorithm>
#include <sstream>

namespace Helianthus::Discovery
{
    ServiceRegistry::ServiceRegistry()
    {
        Config = RegistryConfig{};
        Stats = DiscoveryStats{};
    }

    ServiceRegistry::~ServiceRegistry()
    {
        Shutdown();
    }

    ServiceRegistry::ServiceRegistry(ServiceRegistry&& Other) noexcept
        : Config(std::move(Other.Config))
        , InitializedFlag(Other.InitializedFlag.load())
        , MaintenanceModeFlag(Other.MaintenanceModeFlag.load())
        , ShuttingDownFlag(Other.ShuttingDownFlag.load())
        , Services(std::move(Other.Services))
        , ServicesByName(std::move(Other.ServicesByName))
        , ServiceGroups(std::move(Other.ServiceGroups))
        , GroupsByName(std::move(Other.GroupsByName))
        , NextInstanceId(Other.NextInstanceId.load())
        , NextGroupId(Other.NextGroupId.load())
        , Stats(std::move(Other.Stats))
        , StateChangeCallback(std::move(Other.StateChangeCallback))
        , RegistrationCallback(std::move(Other.RegistrationCallback))
        , StopCleanup(Other.StopCleanup.load())
        , ReplicationEnabled(Other.ReplicationEnabled.load())
        , ReplicaNodes(std::move(Other.ReplicaNodes))
    {
        Other.InitializedFlag = false;
        Other.ShuttingDownFlag = false;
    }

    ServiceRegistry& ServiceRegistry::operator=(ServiceRegistry&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown();
            
            Config = std::move(Other.Config);
            InitializedFlag = Other.InitializedFlag.load();
            MaintenanceModeFlag = Other.MaintenanceModeFlag.load();
            ShuttingDownFlag = Other.ShuttingDownFlag.load();
            Services = std::move(Other.Services);
            ServicesByName = std::move(Other.ServicesByName);
            ServiceGroups = std::move(Other.ServiceGroups);
            GroupsByName = std::move(Other.GroupsByName);
            NextInstanceId = Other.NextInstanceId.load();
            NextGroupId = Other.NextGroupId.load();
            Stats = std::move(Other.Stats);
            StateChangeCallback = std::move(Other.StateChangeCallback);
            RegistrationCallback = std::move(Other.RegistrationCallback);
            StopCleanup = Other.StopCleanup.load();
            ReplicationEnabled = Other.ReplicationEnabled.load();
            ReplicaNodes = std::move(Other.ReplicaNodes);
            
            Other.InitializedFlag = false;
            Other.ShuttingDownFlag = false;
        }
        return *this;
    }

    DiscoveryResult ServiceRegistry::Initialize(const RegistryConfig& Config)
    {
        if (InitializedFlag)
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        this->Config = Config;
        InitializedFlag = true;
        ShuttingDownFlag = false;
        
        // Start cleanup thread if cleanup interval is configured
        if (Config.CleanupIntervalMs > 0)
        {
            StartCleanupThread();
        }

        return DiscoveryResult::SUCCESS;
    }

    void ServiceRegistry::Shutdown()
    {
        if (!InitializedFlag)
        {
            return;
        }

        ShuttingDownFlag = true;
        StopCleanupThread();
        
        std::lock_guard<std::mutex> ServicesLock(ServicesMutex);
        std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
        
        Services.clear();
        ServicesByName.clear();
        ServiceGroups.clear();
        GroupsByName.clear();
        
        InitializedFlag = false;
    }

    bool ServiceRegistry::IsInitialized() const
    {
        return InitializedFlag;
    }

    DiscoveryResult ServiceRegistry::RegisterService(const ServiceInstance& Instance, ServiceInstanceId& OutInstanceId)
    {
        if (!InitializedFlag || MaintenanceModeFlag)
        {
            return DiscoveryResult::INTERNAL_ERROR;
        }

        if (Instance.BaseInfo.ServiceName.empty())
        {
            return DiscoveryResult::INVALID_SERVICE_INFO;
        }

        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        // Check if we've reached the maximum number of services
        if (Services.size() >= Config.MaxServices)
        {
            return DiscoveryResult::REGISTRY_FULL;
        }

        // Generate new instance ID
        OutInstanceId = GenerateInstanceId();
        
        // Create service instance entry
        ServiceInstanceEntry Entry;
        Entry.Instance = std::make_shared<ServiceInstance>(Instance);
        Entry.Instance->InstanceId = OutInstanceId;
        Entry.RegistrationTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        Entry.LastHeartbeat = Entry.RegistrationTime;
        Entry.TtlMs = Config.DefaultTtlMs;

        // Register the service
        Services[OutInstanceId] = Entry;
        ServicesByName[Instance.BaseInfo.ServiceName].push_back(OutInstanceId);

        UpdateStatsOnRegistration();
        NotifyRegistration(OutInstanceId, DiscoveryResult::SUCCESS);

        return DiscoveryResult::SUCCESS;
    }

    DiscoveryResult ServiceRegistry::UpdateService(ServiceInstanceId InstanceId, const ServiceInstance& Instance)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        // Update the instance (keep the same ID)
        auto UpdatedInstance = std::make_shared<ServiceInstance>(Instance);
        UpdatedInstance->InstanceId = InstanceId;
        It->second.Instance = UpdatedInstance;

        return DiscoveryResult::SUCCESS;
    }

    DiscoveryResult ServiceRegistry::DeregisterService(ServiceInstanceId InstanceId)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        // Remove from name-based lookup
        const std::string& ServiceName = It->second.Instance->BaseInfo.ServiceName;
        auto& ServiceList = ServicesByName[ServiceName];
        ServiceList.erase(
            std::remove(ServiceList.begin(), ServiceList.end(), InstanceId),
            ServiceList.end()
        );

        if (ServiceList.empty())
        {
            ServicesByName.erase(ServiceName);
        }

        // Remove the service
        Services.erase(It);
        UpdateStatsOnDeregistration();

        return DiscoveryResult::SUCCESS;
    }

    DiscoveryResult ServiceRegistry::DeregisterServiceByName(const std::string& ServiceName)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = ServicesByName.find(ServiceName);
        if (It == ServicesByName.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        // Remove all instances of this service
        for (auto InstanceId : It->second)
        {
            Services.erase(InstanceId);
            UpdateStatsOnDeregistration();
        }

        ServicesByName.erase(It);
        return DiscoveryResult::SUCCESS;
    }

    ServiceInstancePtr ServiceRegistry::GetService(ServiceInstanceId InstanceId) const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return nullptr;
        }

        return It->second.Instance;
    }

    std::vector<ServiceInstancePtr> ServiceRegistry::GetServicesByName(const std::string& ServiceName) const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        std::vector<ServiceInstancePtr> Result;
        auto It = ServicesByName.find(ServiceName);
        
        if (It != ServicesByName.end())
        {
            for (auto InstanceId : It->second)
            {
                auto ServiceIt = Services.find(InstanceId);
                if (ServiceIt != Services.end())
                {
                    Result.push_back(ServiceIt->second.Instance);
                }
            }
        }

        return Result;
    }

    std::vector<ServiceInstancePtr> ServiceRegistry::GetHealthyServices(const std::string& ServiceName) const
    {
        auto AllServices = GetServicesByName(ServiceName);
        std::vector<ServiceInstancePtr> HealthyServices;

        for (const auto& Service : AllServices)
        {
            if (Service->IsHealthy())
            {
                HealthyServices.push_back(Service);
            }
        }

        return HealthyServices;
    }

    std::vector<ServiceInstancePtr> ServiceRegistry::GetAllServices() const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        std::vector<ServiceInstancePtr> Result;
        Result.reserve(Services.size());

        for (const auto& Pair : Services)
        {
            Result.push_back(Pair.second.Instance);
        }

        return Result;
    }

    std::vector<std::string> ServiceRegistry::GetServiceNames() const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        std::vector<std::string> Result;
        Result.reserve(ServicesByName.size());

        for (const auto& Pair : ServicesByName)
        {
            Result.push_back(Pair.first);
        }

        return Result;
    }

    // Additional core methods continue...
    DiscoveryResult ServiceRegistry::SendHeartbeat(ServiceInstanceId InstanceId)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        It->second.LastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        return DiscoveryResult::SUCCESS;
    }

    void ServiceRegistry::CleanupExpiredServices()
    {
        CleanupExpiredServicesInternal();
    }

    uint32_t ServiceRegistry::GetServiceCount() const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        return static_cast<uint32_t>(ServicesByName.size());
    }

    uint32_t ServiceRegistry::GetServiceInstanceCount() const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        return static_cast<uint32_t>(Services.size());
    }

    // Private helper methods
    ServiceInstanceId ServiceRegistry::GenerateInstanceId()
    {
        return NextInstanceId.fetch_add(1, std::memory_order_relaxed);
    }

    ServiceGroupId ServiceRegistry::GenerateGroupId()
    {
        return NextGroupId.fetch_add(1, std::memory_order_relaxed);
    }

    void ServiceRegistry::UpdateStatsOnRegistration()
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        Stats.RegistrationCount++;
        Stats.TotalServices = ServicesByName.size();
        Stats.TotalServiceInstances = Services.size();
    }

    void ServiceRegistry::UpdateStatsOnDeregistration()
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        Stats.DeregistrationCount++;
        Stats.TotalServices = ServicesByName.size();
        Stats.TotalServiceInstances = Services.size();
    }

    void ServiceRegistry::CleanupExpiredServicesInternal()
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        std::vector<ServiceInstanceId> ExpiredServices;
        
        for (const auto& Pair : Services)
        {
            if (Pair.second.IsExpired())
            {
                ExpiredServices.push_back(Pair.first);
            }
        }

        // Remove expired services
        for (auto InstanceId : ExpiredServices)
        {
            DeregisterService(InstanceId); // This will handle the cleanup
        }
    }

    void ServiceRegistry::StartCleanupThread()
    {
        StopCleanup = false;
        CleanupThread = std::thread(&ServiceRegistry::CleanupLoop, this);
    }

    void ServiceRegistry::StopCleanupThread()
    {
        StopCleanup = true;
        if (CleanupThread.joinable())
        {
            CleanupThread.join();
        }
    }

    void ServiceRegistry::CleanupLoop()
    {
        while (!StopCleanup && !ShuttingDownFlag)
        {
            CleanupExpiredServicesInternal();
            std::this_thread::sleep_for(std::chrono::milliseconds(Config.CleanupIntervalMs));
        }
    }

    // Placeholder implementations for remaining interface methods
    std::vector<ServiceInstancePtr> ServiceRegistry::FindServices(
        const std::string& ServiceName,
        const std::unordered_map<std::string, std::string>& Tags,
        const std::string& Region,
        const std::string& Zone,
        ServiceState MinState
    ) const
    {
        // Basic implementation - get all services by name and filter
        auto Services = GetServicesByName(ServiceName);
        std::vector<ServiceInstancePtr> FilteredServices;

        for (const auto& Service : Services)
        {
            if (MatchesFilters(Service, Tags, Region, Zone, MinState))
            {
                FilteredServices.push_back(Service);
            }
        }

        return FilteredServices;
    }

    bool ServiceRegistry::MatchesFilters(const ServiceInstancePtr& Instance,
                                       const std::unordered_map<std::string, std::string>& Tags,
                                       const std::string& Region,
                                       const std::string& Zone,
                                       ServiceState MinState) const
    {
        // Check state
        if (static_cast<int>(Instance->State) < static_cast<int>(MinState))
        {
            return false;
        }

        // Check region
        if (!Region.empty() && Instance->Region != Region)
        {
            return false;
        }

        // Check zone
        if (!Zone.empty() && Instance->Zone != Zone)
        {
            return false;
        }

        // Check tags (simplified)
        for (const auto& TagPair : Tags)
        {
            auto It = Instance->Tags.find(TagPair.first);
            if (It == Instance->Tags.end() || It->second != TagPair.second)
            {
                return false;
            }
        }

        return true;
    }

    // More placeholder implementations for remaining methods...
    std::vector<ServiceInstancePtr> ServiceRegistry::FindServicesByTag(const std::string& TagKey, const std::string& TagValue) const
    {
        std::vector<ServiceInstancePtr> Result;
        auto AllServices = GetAllServices();

        for (const auto& Service : AllServices)
        {
            auto It = Service->Tags.find(TagKey);
            if (It != Service->Tags.end() && (TagValue.empty() || It->second == TagValue))
            {
                Result.push_back(Service);
            }
        }

        return Result;
    }

    std::vector<ServiceInstancePtr> ServiceRegistry::FindServicesByRegion(const std::string& Region) const
    {
        std::vector<ServiceInstancePtr> Result;
        auto AllServices = GetAllServices();

        for (const auto& Service : AllServices)
        {
            if (Service->Region == Region)
            {
                Result.push_back(Service);
            }
        }

        return Result;
    }

    std::vector<ServiceInstancePtr> ServiceRegistry::FindServicesByZone(const std::string& Zone) const
    {
        std::vector<ServiceInstancePtr> Result;
        auto AllServices = GetAllServices();

        for (const auto& Service : AllServices)
        {
            if (Service->Zone == Zone)
            {
                Result.push_back(Service);
            }
        }

        return Result;
    }

    DiscoveryResult ServiceRegistry::UpdateServiceState(ServiceInstanceId InstanceId, ServiceState State)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        ServiceState OldState = It->second.Instance->State;
        It->second.Instance->State = State;
        
        NotifyStateChange(InstanceId, OldState, State);
        return DiscoveryResult::SUCCESS;
    }

    DiscoveryResult ServiceRegistry::UpdateServiceHealth(ServiceInstanceId InstanceId, HealthScore Score)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        It->second.Instance->CurrentHealthScore = Score;
        return DiscoveryResult::SUCCESS;
    }

    DiscoveryResult ServiceRegistry::UpdateServiceLoad(ServiceInstanceId InstanceId, uint32_t ActiveConnections)
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return DiscoveryResult::SERVICE_NOT_FOUND;
        }

        It->second.Instance->ActiveConnections = ActiveConnections;
        return DiscoveryResult::SUCCESS;
    }

    ServiceState ServiceRegistry::GetServiceState(ServiceInstanceId InstanceId) const
    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        
        auto It = Services.find(InstanceId);
        if (It == Services.end())
        {
            return ServiceState::UNKNOWN;
        }

        return It->second.Instance->State;
    }

    // Callback and notification methods
    void ServiceRegistry::NotifyStateChange(ServiceInstanceId InstanceId, ServiceState OldState, ServiceState NewState)
    {
        if (StateChangeCallback)
        {
            StateChangeCallback(InstanceId, OldState, NewState);
        }
    }

    void ServiceRegistry::NotifyRegistration(ServiceInstanceId InstanceId, DiscoveryResult Result)
    {
        if (RegistrationCallback)
        {
            RegistrationCallback(InstanceId, Result);
        }
    }

    // Simple stub implementations for other methods
    DiscoveryResult ServiceRegistry::SetServiceTtl(ServiceInstanceId InstanceId, uint32_t TtlMs) { return DiscoveryResult::SUCCESS; }
    DiscoveryResult ServiceRegistry::RenewService(ServiceInstanceId InstanceId) { return SendHeartbeat(InstanceId); }
    DiscoveryResult ServiceRegistry::CreateServiceGroup(const std::string& ServiceName, const LoadBalanceConfig& Config, ServiceGroupId& OutGroupId) { return DiscoveryResult::SUCCESS; }
    DiscoveryResult ServiceRegistry::UpdateServiceGroup(ServiceGroupId GroupId, const LoadBalanceConfig& Config) { return DiscoveryResult::SUCCESS; }
    DiscoveryResult ServiceRegistry::DeleteServiceGroup(ServiceGroupId GroupId) { return DiscoveryResult::SUCCESS; }
    ServiceGroupPtr ServiceRegistry::GetServiceGroup(ServiceGroupId GroupId) const { return nullptr; }
    ServiceGroupPtr ServiceRegistry::GetServiceGroupByName(const std::string& ServiceName) const { return nullptr; }
    DiscoveryStats ServiceRegistry::GetRegistryStats() const { return Stats; }
    uint32_t ServiceRegistry::GetHealthyServiceCount() const { return 0; }
    std::unordered_map<std::string, uint32_t> ServiceRegistry::GetServiceCountByName() const { return {}; }
    void ServiceRegistry::UpdateConfig(const RegistryConfig& Config) { this->Config = Config; }
    RegistryConfig ServiceRegistry::GetCurrentConfig() const { return Config; }
    void ServiceRegistry::RefreshRegistry() {}
    DiscoveryResult ServiceRegistry::ValidateRegistry() { return DiscoveryResult::SUCCESS; }
    DiscoveryResult ServiceRegistry::SaveRegistryState() { return DiscoveryResult::SUCCESS; }
    DiscoveryResult ServiceRegistry::LoadRegistryState() { return DiscoveryResult::SUCCESS; }
    DiscoveryResult ServiceRegistry::ClearPersistedState() { return DiscoveryResult::SUCCESS; }
    void ServiceRegistry::SetServiceStateChangeCallback(ServiceStateChangeCallback Callback) { StateChangeCallback = std::move(Callback); }
    void ServiceRegistry::SetServiceRegistrationCallback(ServiceRegistrationCallback Callback) { RegistrationCallback = std::move(Callback); }
    void ServiceRegistry::RemoveAllCallbacks() { StateChangeCallback = nullptr; RegistrationCallback = nullptr; }
    DiscoveryResult ServiceRegistry::EnableReplication(const std::vector<Network::NetworkAddress>& ReplicaNodes) { return DiscoveryResult::SUCCESS; }
    void ServiceRegistry::DisableReplication() {}
    bool ServiceRegistry::IsReplicationEnabled() const { return ReplicationEnabled; }
    DiscoveryResult ServiceRegistry::SyncWithReplicas() { return DiscoveryResult::SUCCESS; }
    void ServiceRegistry::SetMaintenanceMode(bool Enable) { MaintenanceModeFlag = Enable; }
    bool ServiceRegistry::IsInMaintenanceMode() const { return MaintenanceModeFlag; }
    void ServiceRegistry::ResetRegistry() { Services.clear(); ServicesByName.clear(); ServiceGroups.clear(); GroupsByName.clear(); }
    std::string ServiceRegistry::GetRegistryInfo() const { return "ServiceRegistry"; }

} // namespace Helianthus::Discovery