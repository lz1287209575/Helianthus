#pragma once

#include "DiscoveryTypes.h"
#include "IServiceRegistry.h"
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace Helianthus::Discovery
{
    /**
     * @brief Basic in-memory service registry implementation
     * 
     * Thread-safe service registry that maintains service instances,
     * handles TTL expiration, and provides service discovery functionality.
     */
    class ServiceRegistry : public IServiceRegistry
    {
    public:
        ServiceRegistry();
        virtual ~ServiceRegistry();

        // Disable copy, allow move
        ServiceRegistry(const ServiceRegistry&) = delete;
        ServiceRegistry& operator=(const ServiceRegistry&) = delete;
        ServiceRegistry(ServiceRegistry&& Other) noexcept;
        ServiceRegistry& operator=(ServiceRegistry&& Other) noexcept;

        // IServiceRegistry implementation
        DiscoveryResult Initialize(const RegistryConfig& Config) override;
        void Shutdown() override;
        bool IsInitialized() const override;

        DiscoveryResult RegisterService(const ServiceInstance& Instance, ServiceInstanceId& OutInstanceId) override;
        DiscoveryResult UpdateService(ServiceInstanceId InstanceId, const ServiceInstance& Instance) override;
        DiscoveryResult DeregisterService(ServiceInstanceId InstanceId) override;
        DiscoveryResult DeregisterServiceByName(const std::string& ServiceName) override;

        ServiceInstancePtr GetService(ServiceInstanceId InstanceId) const override;
        std::vector<ServiceInstancePtr> GetServicesByName(const std::string& ServiceName) const override;
        std::vector<ServiceInstancePtr> GetHealthyServices(const std::string& ServiceName) const override;
        std::vector<ServiceInstancePtr> GetAllServices() const override;
        std::vector<std::string> GetServiceNames() const override;

        std::vector<ServiceInstancePtr> FindServices(
            const std::string& ServiceName,
            const std::unordered_map<std::string, std::string>& Tags = {},
            const std::string& Region = "",
            const std::string& Zone = "",
            ServiceState MinState = ServiceState::HEALTHY
        ) const override;

        std::vector<ServiceInstancePtr> FindServicesByTag(
            const std::string& TagKey,
            const std::string& TagValue = ""
        ) const override;

        std::vector<ServiceInstancePtr> FindServicesByRegion(const std::string& Region) const override;
        std::vector<ServiceInstancePtr> FindServicesByZone(const std::string& Zone) const override;

        DiscoveryResult UpdateServiceState(ServiceInstanceId InstanceId, ServiceState State) override;
        DiscoveryResult UpdateServiceHealth(ServiceInstanceId InstanceId, HealthScore Score) override;
        DiscoveryResult UpdateServiceLoad(ServiceInstanceId InstanceId, uint32_t ActiveConnections) override;
        ServiceState GetServiceState(ServiceInstanceId InstanceId) const override;

        DiscoveryResult SendHeartbeat(ServiceInstanceId InstanceId) override;
        DiscoveryResult SetServiceTtl(ServiceInstanceId InstanceId, uint32_t TtlMs) override;
        DiscoveryResult RenewService(ServiceInstanceId InstanceId) override;
        void CleanupExpiredServices() override;

        DiscoveryResult CreateServiceGroup(const std::string& ServiceName, const LoadBalanceConfig& Config, ServiceGroupId& OutGroupId) override;
        DiscoveryResult UpdateServiceGroup(ServiceGroupId GroupId, const LoadBalanceConfig& Config) override;
        DiscoveryResult DeleteServiceGroup(ServiceGroupId GroupId) override;
        ServiceGroupPtr GetServiceGroup(ServiceGroupId GroupId) const override;
        ServiceGroupPtr GetServiceGroupByName(const std::string& ServiceName) const override;

        DiscoveryStats GetRegistryStats() const override;
        uint32_t GetServiceCount() const override;
        uint32_t GetServiceInstanceCount() const override;
        uint32_t GetHealthyServiceCount() const override;
        std::unordered_map<std::string, uint32_t> GetServiceCountByName() const override;

        void UpdateConfig(const RegistryConfig& Config) override;
        RegistryConfig GetCurrentConfig() const override;
        void RefreshRegistry() override;
        DiscoveryResult ValidateRegistry() override;

        DiscoveryResult SaveRegistryState() override;
        DiscoveryResult LoadRegistryState() override;
        DiscoveryResult ClearPersistedState() override;

        void SetServiceStateChangeCallback(ServiceStateChangeCallback Callback) override;
        void SetServiceRegistrationCallback(ServiceRegistrationCallback Callback) override;
        void RemoveAllCallbacks() override;

        DiscoveryResult EnableReplication(const std::vector<Network::NetworkAddress>& ReplicaNodes) override;
        void DisableReplication() override;
        bool IsReplicationEnabled() const override;
        DiscoveryResult SyncWithReplicas() override;

        void SetMaintenanceMode(bool Enable) override;
        bool IsInMaintenanceMode() const override;
        void ResetRegistry() override;
        std::string GetRegistryInfo() const override;

    private:
        // Internal data structures
        struct ServiceInstanceEntry
        {
            ServiceInstancePtr Instance;
            Common::TimestampMs RegistrationTime;
            Common::TimestampMs LastHeartbeat;
            uint32_t TtlMs;
            bool IsExpired() const
            {
                auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                return (Now - LastHeartbeat) > TtlMs;
            }
        };

        // Helper methods
        ServiceInstanceId GenerateInstanceId();
        ServiceGroupId GenerateGroupId();
        bool MatchesFilters(const ServiceInstancePtr& Instance,
                           const std::unordered_map<std::string, std::string>& Tags,
                           const std::string& Region,
                           const std::string& Zone,
                           ServiceState MinState) const;
        void UpdateStatsOnRegistration();
        void UpdateStatsOnDeregistration();
        void CleanupExpiredServicesInternal();
        void NotifyStateChange(ServiceInstanceId InstanceId, ServiceState OldState, ServiceState NewState);
        void NotifyRegistration(ServiceInstanceId InstanceId, DiscoveryResult Result);
        void StartCleanupThread();
        void StopCleanupThread();
        void CleanupLoop();

    private:
        // Configuration and state
        RegistryConfig Config;
        std::atomic<bool> InitializedFlag = false;
        std::atomic<bool> MaintenanceModeFlag = false;
        std::atomic<bool> ShuttingDownFlag = false;

        // Service storage
        mutable std::mutex ServicesMutex;
        std::unordered_map<ServiceInstanceId, ServiceInstanceEntry> Services;
        std::unordered_map<std::string, std::vector<ServiceInstanceId>> ServicesByName;
        
        // Service groups
        mutable std::mutex GroupsMutex;
        std::unordered_map<ServiceGroupId, ServiceGroupPtr> ServiceGroups;
        std::unordered_map<std::string, ServiceGroupId> GroupsByName;

        // ID generation
        std::atomic<ServiceInstanceId> NextInstanceId = 1;
        std::atomic<ServiceGroupId> NextGroupId = 1;

        // Statistics
        mutable std::mutex StatsMutex;
        DiscoveryStats Stats;

        // Callbacks
        ServiceStateChangeCallback StateChangeCallback;
        ServiceRegistrationCallback RegistrationCallback;

        // Cleanup thread
        std::thread CleanupThread;
        std::atomic<bool> StopCleanup = false;

        // Replication (placeholder)
        std::atomic<bool> ReplicationEnabled = false;
        std::vector<Network::NetworkAddress> ReplicaNodes;
    };

} // namespace Helianthus::Discovery