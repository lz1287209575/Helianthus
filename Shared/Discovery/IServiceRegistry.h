#pragma once

#include "DiscoveryTypes.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Helianthus::Discovery
{
    /**
     * @brief Abstract interface for service registry
     * 
     * The service registry maintains a centralized catalog of all available
     * services in the distributed system, their endpoints, health status,
     * and metadata for service discovery and load balancing.
     */
    class IServiceRegistry
    {
    public:
        virtual ~IServiceRegistry() = default;

        // Initialization and lifecycle
        virtual DiscoveryResult Initialize(const RegistryConfig& Config) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsInitialized() const = 0;

        // Service registration
        virtual DiscoveryResult RegisterService(const ServiceInstance& Instance, ServiceInstanceId& OutInstanceId) = 0;
        virtual DiscoveryResult UpdateService(ServiceInstanceId InstanceId, const ServiceInstance& Instance) = 0;
        virtual DiscoveryResult DeregisterService(ServiceInstanceId InstanceId) = 0;
        virtual DiscoveryResult DeregisterServiceByName(const std::string& ServiceName) = 0;

        // Service information retrieval
        virtual ServiceInstancePtr GetService(ServiceInstanceId InstanceId) const = 0;
        virtual std::vector<ServiceInstancePtr> GetServicesByName(const std::string& ServiceName) const = 0;
        virtual std::vector<ServiceInstancePtr> GetHealthyServices(const std::string& ServiceName) const = 0;
        virtual std::vector<ServiceInstancePtr> GetAllServices() const = 0;
        virtual std::vector<std::string> GetServiceNames() const = 0;

        // Service filtering and querying
        virtual std::vector<ServiceInstancePtr> FindServices(
            const std::string& ServiceName,
            const std::unordered_map<std::string, std::string>& Tags = {},
            const std::string& Region = "",
            const std::string& Zone = "",
            SERVICE_STATE MinState = SERVICE_STATE::HEALTHY
        ) const = 0;

        virtual std::vector<ServiceInstancePtr> FindServicesByTag(
            const std::string& TagKey,
            const std::string& TagValue = ""
        ) const = 0;

        virtual std::vector<ServiceInstancePtr> FindServicesByRegion(const std::string& Region) const = 0;
        virtual std::vector<ServiceInstancePtr> FindServicesByZone(const std::string& Zone) const = 0;

        // Service state management
        virtual DiscoveryResult UpdateServiceState(ServiceInstanceId InstanceId, SERVICE_STATE State) = 0;
        virtual DiscoveryResult UpdateServiceHealth(ServiceInstanceId InstanceId, HealthScore Score) = 0;
        virtual DiscoveryResult UpdateServiceLoad(ServiceInstanceId InstanceId, uint32_t ActiveConnections) = 0;
        virtual SERVICE_STATE GetServiceState(ServiceInstanceId InstanceId) const = 0;

        // Heartbeat and TTL management
        virtual DiscoveryResult SendHeartbeat(ServiceInstanceId InstanceId) = 0;
        virtual DiscoveryResult SetServiceTtl(ServiceInstanceId InstanceId, uint32_t TtlMs) = 0;
        virtual DiscoveryResult RenewService(ServiceInstanceId InstanceId) = 0;
        virtual void CleanupExpiredServices() = 0;

        // Service groups and load balancing
        virtual DiscoveryResult CreateServiceGroup(const std::string& ServiceName, const LoadBalanceConfig& Config, ServiceGroupId& OutGroupId) = 0;
        virtual DiscoveryResult UpdateServiceGroup(ServiceGroupId GroupId, const LoadBalanceConfig& Config) = 0;
        virtual DiscoveryResult DeleteServiceGroup(ServiceGroupId GroupId) = 0;
        virtual ServiceGroupPtr GetServiceGroup(ServiceGroupId GroupId) const = 0;
        virtual ServiceGroupPtr GetServiceGroupByName(const std::string& ServiceName) const = 0;

        // Statistics and monitoring
        virtual DiscoveryStats GetRegistryStats() const = 0;
        virtual uint32_t GetServiceCount() const = 0;
        virtual uint32_t GetServiceInstanceCount() const = 0;
        virtual uint32_t GetHealthyServiceCount() const = 0;
        virtual std::unordered_map<std::string, uint32_t> GetServiceCountByName() const = 0;

        // Configuration and maintenance
        virtual void UpdateConfig(const RegistryConfig& Config) = 0;
        virtual RegistryConfig GetCurrentConfig() const = 0;
        virtual void RefreshRegistry() = 0;
        virtual DiscoveryResult ValidateRegistry() = 0;

        // Persistence (if enabled)
        virtual DiscoveryResult SaveRegistryState() = 0;
        virtual DiscoveryResult LoadRegistryState() = 0;
        virtual DiscoveryResult ClearPersistedState() = 0;

        // Event callbacks
        virtual void SetServiceStateChangeCallback(ServiceStateChangeCallback Callback) = 0;
        virtual void SetServiceRegistrationCallback(ServiceRegistrationCallback Callback) = 0;
        virtual void RemoveAllCallbacks() = 0;

        // Replication (if enabled)
        virtual DiscoveryResult EnableReplication(const std::vector<Network::NetworkAddress>& ReplicaNodes) = 0;
        virtual void DisableReplication() = 0;
        virtual bool IsReplicationEnabled() const = 0;
        virtual DiscoveryResult SyncWithReplicas() = 0;

        // Administrative functions
        virtual void SetMaintenanceMode(bool Enable) = 0;
        virtual bool IsInMaintenanceMode() const = 0;
        virtual void ResetRegistry() = 0;
        virtual std::string GetRegistryInfo() const = 0;
    };

} // namespace Helianthus::Discovery
