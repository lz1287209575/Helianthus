#pragma once

#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "DiscoveryTypes.h"

namespace Helianthus::Discovery
{
/**
 * @brief Abstract interface for service discovery
 *
 * The service discovery component provides client-side functionality
 * for finding and connecting to available services, including caching,
 * failover, and intelligent routing capabilities.
 */
class IServiceDiscovery
{
public:
    virtual ~IServiceDiscovery() = default;

    // Initialization and lifecycle
    virtual DiscoveryResult
    Initialize(const std::vector<Network::NetworkAddress>& RegistryEndpoints) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Basic service discovery
    virtual ServiceInstancePtr DiscoverService(const std::string& ServiceName) = 0;
    virtual std::vector<ServiceInstancePtr> DiscoverAllServices(const std::string& ServiceName) = 0;
    virtual ServiceInstancePtr
    DiscoverServiceByTag(const std::string& ServiceName,
                         const std::unordered_map<std::string, std::string>& Tags) = 0;
    virtual ServiceInstancePtr DiscoverServiceByRegion(const std::string& ServiceName,
                                                       const std::string& Region) = 0;
    virtual ServiceInstancePtr DiscoverServiceByZone(const std::string& ServiceName,
                                                     const std::string& Zone) = 0;

    // Asynchronous discovery
    virtual std::future<ServiceInstancePtr>
    DiscoverServiceAsync(const std::string& ServiceName) = 0;
    virtual std::future<std::vector<ServiceInstancePtr>>
    DiscoverAllServicesAsync(const std::string& ServiceName) = 0;

    // Service watching and subscriptions
    virtual DiscoveryResult WatchService(const std::string& ServiceName,
                                         ServiceDiscoveryCallback Callback) = 0;
    virtual DiscoveryResult UnwatchService(const std::string& ServiceName) = 0;
    virtual DiscoveryResult WatchAllServices(ServiceDiscoveryCallback Callback) = 0;
    virtual void UnwatchAllServices() = 0;

    // Connection management
    virtual Network::ConnectionId
    ConnectToService(const std::string& ServiceName,
                     Network::ProtocolType Protocol = Network::ProtocolType::TCP) = 0;
    virtual Network::ConnectionId
    ConnectToServiceInstance(ServiceInstanceId InstanceId,
                             Network::ProtocolType Protocol = Network::ProtocolType::TCP) = 0;
    virtual void DisconnectFromService(Network::ConnectionId ConnectionId) = 0;
    virtual std::vector<Network::ConnectionId>
    GetActiveConnections(const std::string& ServiceName) const = 0;

    // Health monitoring and failover
    virtual bool IsServiceHealthy(const std::string& ServiceName) = 0;
    virtual bool IsServiceInstanceHealthy(ServiceInstanceId InstanceId) = 0;
    virtual void MarkServiceInstanceFailed(ServiceInstanceId InstanceId) = 0;
    virtual void EnableFailover(const std::string& ServiceName, bool Enable = true) = 0;
    virtual bool IsFailoverEnabled(const std::string& ServiceName) const = 0;

    // Load balancing integration
    virtual ServiceInstancePtr
    SelectServiceInstance(const std::string& ServiceName,
                          LoadBalanceStrategy Strategy = LoadBalanceStrategy::ROUND_ROBIN) = 0;
    virtual ServiceInstancePtr SelectServiceInstanceWithContext(
        const std::string& ServiceName,
        const std::string& Context,
        LoadBalanceStrategy Strategy = LoadBalanceStrategy::ROUND_ROBIN) = 0;
    virtual void UpdateLoadBalanceStrategy(const std::string& ServiceName,
                                           LoadBalanceStrategy Strategy) = 0;

    // Caching and performance
    virtual void EnableCaching(bool Enable = true) = 0;
    virtual bool IsCachingEnabled() const = 0;
    virtual void SetCacheTtl(uint32_t TtlMs) = 0;
    virtual uint32_t GetCacheTtl() const = 0;
    virtual void ClearCache() = 0;
    virtual void ClearCacheForService(const std::string& ServiceName) = 0;
    virtual void RefreshCache() = 0;

    // Registry connectivity
    virtual DiscoveryResult ConnectToRegistry(const Network::NetworkAddress& RegistryEndpoint) = 0;
    virtual void DisconnectFromRegistry() = 0;
    virtual void DisconnectFromAllRegistries() = 0;
    virtual bool IsConnectedToRegistry() const = 0;
    virtual std::vector<Network::NetworkAddress> GetConnectedRegistries() const = 0;
    virtual Network::NetworkAddress GetPrimaryRegistry() const = 0;

    // Configuration and preferences
    virtual void SetPreferredRegion(const std::string& Region) = 0;
    virtual std::string GetPreferredRegion() const = 0;
    virtual void SetPreferredZone(const std::string& Zone) = 0;
    virtual std::string GetPreferredZone() const = 0;
    virtual void SetConnectionTimeout(uint32_t TimeoutMs) = 0;
    virtual uint32_t GetConnectionTimeout() const = 0;

    // Service filtering and policies
    virtual void
    AddServiceFilter(const std::string& ServiceName,
                     const std::unordered_map<std::string, std::string>& RequiredTags) = 0;
    virtual void RemoveServiceFilter(const std::string& ServiceName) = 0;
    virtual void ClearServiceFilters() = 0;
    virtual void SetDiscoveryPolicy(const std::string& ServiceName, const std::string& Policy) = 0;

    // Statistics and monitoring
    virtual DiscoveryStats GetDiscoveryStats() const = 0;
    virtual uint32_t GetCachedServiceCount() const = 0;
    virtual uint32_t GetActiveConnectionCount() const = 0;
    virtual std::unordered_map<std::string, uint32_t> GetConnectionCountByService() const = 0;
    virtual Common::TimestampMs GetLastDiscoveryTime() const = 0;

    // Debugging and diagnostics
    virtual std::string GetDiscoveryInfo() const = 0;
    virtual std::vector<std::string> GetKnownServices() const = 0;
    virtual void EnableDebugLogging(bool Enable = true) = 0;
    virtual bool IsDebugLoggingEnabled() const = 0;

    // Event callbacks
    virtual void SetServiceDiscoveryCallback(ServiceDiscoveryCallback Callback) = 0;
    virtual void
    SetConnectionCallback(std::function<void(Network::ConnectionId, bool)> Callback) = 0;
    virtual void RemoveAllCallbacks() = 0;

    // Circuit breaker integration
    virtual void EnableCircuitBreaker(const std::string& ServiceName,
                                      uint32_t FailureThreshold = 5,
                                      uint32_t RecoveryTimeMs = 30000) = 0;
    virtual void DisableCircuitBreaker(const std::string& ServiceName) = 0;
    virtual bool IsCircuitBreakerOpen(const std::string& ServiceName) const = 0;
    virtual void ResetCircuitBreaker(const std::string& ServiceName) = 0;

    // Batch operations
    virtual std::unordered_map<std::string, ServiceInstancePtr>
    DiscoverMultipleServices(const std::vector<std::string>& ServiceNames) = 0;
    virtual DiscoveryResult ConnectToMultipleServices(
        const std::vector<std::string>& ServiceNames,
        std::unordered_map<std::string, Network::ConnectionId>& OutConnections) = 0;
};

}  // namespace Helianthus::Discovery
