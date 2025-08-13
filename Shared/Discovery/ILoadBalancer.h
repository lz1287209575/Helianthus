#pragma once

#include "DiscoveryTypes.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Helianthus::Discovery
{
    /**
     * @brief Abstract interface for load balancer
     * 
     * The load balancer distributes incoming requests across multiple
     * service instances based on various strategies such as round-robin,
     * least connections, weighted distribution, and health-aware routing.
     */
    class ILoadBalancer
    {
    public:
        virtual ~ILoadBalancer() = default;

        // Initialization and lifecycle
        virtual DiscoveryResult Initialize(const LoadBalanceConfig& Config) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsInitialized() const = 0;

        // Service instance management
        virtual DiscoveryResult AddServiceInstance(ServiceInstancePtr Instance) = 0;
        virtual DiscoveryResult RemoveServiceInstance(ServiceInstanceId InstanceId) = 0;
        virtual DiscoveryResult UpdateServiceInstance(ServiceInstancePtr Instance) = 0;
        virtual void ClearServiceInstances(const std::string& ServiceName = "") = 0;
        virtual std::vector<ServiceInstancePtr> GetServiceInstances(const std::string& ServiceName) const = 0;
        virtual uint32_t GetServiceInstanceCount(const std::string& ServiceName) const = 0;

        // Load balancing selection
        virtual ServiceInstancePtr SelectInstance(const std::string& ServiceName) = 0;
        virtual ServiceInstancePtr SelectInstanceWithStrategy(const std::string& ServiceName, LoadBalanceStrategy Strategy) = 0;
        virtual ServiceInstancePtr SelectInstanceWithContext(const std::string& ServiceName, const std::string& Context) = 0;
        virtual ServiceInstancePtr SelectInstanceWithWeight(const std::string& ServiceName, LoadWeight MinWeight = 0) = 0;
        virtual ServiceInstancePtr SelectHealthiestInstance(const std::string& ServiceName) = 0;

        // Strategy configuration
        virtual void SetLoadBalanceStrategy(const std::string& ServiceName, LoadBalanceStrategy Strategy) = 0;
        virtual LoadBalanceStrategy GetLoadBalanceStrategy(const std::string& ServiceName) const = 0;
        virtual void SetDefaultStrategy(LoadBalanceStrategy Strategy) = 0;
        virtual LoadBalanceStrategy GetDefaultStrategy() const = 0;

        // Weight management
        virtual DiscoveryResult SetInstanceWeight(ServiceInstanceId InstanceId, LoadWeight Weight) = 0;
        virtual LoadWeight GetInstanceWeight(ServiceInstanceId InstanceId) const = 0;
        virtual void SetDefaultWeight(LoadWeight Weight) = 0;
        virtual LoadWeight GetDefaultWeight() const = 0;
        virtual void RebalanceWeights(const std::string& ServiceName) = 0;

        // Connection tracking
        virtual DiscoveryResult RecordConnection(ServiceInstanceId InstanceId) = 0;
        virtual DiscoveryResult RecordDisconnection(ServiceInstanceId InstanceId) = 0;
        virtual uint32_t GetActiveConnections(ServiceInstanceId InstanceId) const = 0;
        virtual uint32_t GetTotalActiveConnections(const std::string& ServiceName) const = 0;
        virtual void ResetConnectionCounts(const std::string& ServiceName = "") = 0;

        // Health-aware load balancing
        virtual void UpdateInstanceHealth(ServiceInstanceId InstanceId, HealthScore Score) = 0;
        virtual HealthScore GetInstanceHealth(ServiceInstanceId InstanceId) const = 0;
        virtual void SetHealthThreshold(HealthScore MinHealthScore) = 0;
        virtual HealthScore GetHealthThreshold() const = 0;
        virtual std::vector<ServiceInstancePtr> GetHealthyInstances(const std::string& ServiceName) const = 0;

        // Response time tracking
        virtual void RecordResponseTime(ServiceInstanceId InstanceId, uint32_t ResponseTimeMs) = 0;
        virtual uint32_t GetAverageResponseTime(ServiceInstanceId InstanceId) const = 0;
        virtual ServiceInstancePtr GetFastestInstance(const std::string& ServiceName) = 0;
        virtual void ResetResponseTimes(const std::string& ServiceName = "") = 0;

        // Sticky sessions
        virtual void EnableStickySession(const std::string& ServiceName, const std::string& SessionKey) = 0;
        virtual void DisableStickySession(const std::string& ServiceName) = 0;
        virtual bool IsStickySessionEnabled(const std::string& ServiceName) const = 0;
        virtual ServiceInstancePtr GetStickyInstance(const std::string& ServiceName, const std::string& SessionId) = 0;
        virtual DiscoveryResult BindSession(const std::string& ServiceName, const std::string& SessionId, ServiceInstanceId InstanceId) = 0;
        virtual void UnbindSession(const std::string& ServiceName, const std::string& SessionId) = 0;

        // Consistent hashing
        virtual void EnableConsistentHashing(const std::string& ServiceName, uint32_t VirtualNodes = 150) = 0;
        virtual void DisableConsistentHashing(const std::string& ServiceName) = 0;
        virtual bool IsConsistentHashingEnabled(const std::string& ServiceName) const = 0;
        virtual ServiceInstancePtr GetConsistentHashInstance(const std::string& ServiceName, const std::string& Key) = 0;
        virtual void UpdateHashRing(const std::string& ServiceName) = 0;

        // Circuit breaker integration
        virtual void MarkInstanceFailed(ServiceInstanceId InstanceId) = 0;
        virtual void MarkInstanceRecovered(ServiceInstanceId InstanceId) = 0;
        virtual bool IsInstanceFailed(ServiceInstanceId InstanceId) const = 0;
        virtual void SetFailureThreshold(ServiceInstanceId InstanceId, uint32_t Threshold) = 0;
        virtual void ResetFailureCount(ServiceInstanceId InstanceId) = 0;

        // Load metrics and statistics
        virtual float GetLoadFactor(ServiceInstanceId InstanceId) const = 0;
        virtual float GetServiceLoadFactor(const std::string& ServiceName) const = 0;
        virtual std::unordered_map<ServiceInstanceId, uint32_t> GetLoadDistribution(const std::string& ServiceName) const = 0;
        virtual void UpdateLoadMetrics(ServiceInstanceId InstanceId, float CpuUsage, float MemoryUsage, float NetworkUsage) = 0;

        // Configuration management
        virtual void UpdateConfig(const LoadBalanceConfig& Config) = 0;
        virtual LoadBalanceConfig GetCurrentConfig() const = 0;
        virtual void SetMaxConnections(ServiceInstanceId InstanceId, uint32_t MaxConnections) = 0;
        virtual uint32_t GetMaxConnections(ServiceInstanceId InstanceId) const = 0;

        // Failover and redundancy
        virtual void EnableFailover(const std::string& ServiceName, bool Enable = true) = 0;
        virtual bool IsFailoverEnabled(const std::string& ServiceName) const = 0;
        virtual void SetFailoverPriority(ServiceInstanceId InstanceId, uint32_t Priority) = 0;
        virtual ServiceInstancePtr GetFailoverInstance(const std::string& ServiceName) = 0;

        // Geographic preferences
        virtual void SetPreferredRegion(const std::string& ServiceName, const std::string& Region) = 0;
        virtual std::string GetPreferredRegion(const std::string& ServiceName) const = 0;
        virtual void SetPreferredZone(const std::string& ServiceName, const std::string& Zone) = 0;
        virtual std::string GetPreferredZone(const std::string& ServiceName) const = 0;
        virtual ServiceInstancePtr SelectInstanceByLocation(const std::string& ServiceName, const std::string& Region, const std::string& Zone) = 0;

        // Statistics and monitoring
        virtual std::unordered_map<std::string, uint64_t> GetSelectionStats() const = 0;
        virtual uint64_t GetTotalSelections(const std::string& ServiceName) const = 0;
        virtual void ResetSelectionStats(const std::string& ServiceName = "") = 0;
        virtual std::string GetLoadBalancerInfo() const = 0;

        // Event callbacks
        virtual void SetLoadBalanceCallback(LoadBalanceCallback Callback) = 0;
        virtual void SetInstanceFailureCallback(std::function<void(ServiceInstanceId InstanceId, const std::string& Reason)> Callback) = 0;
        virtual void RemoveAllCallbacks() = 0;

        // Advanced features
        virtual void EnableAdaptiveBalancing(const std::string& ServiceName, bool Enable = true) = 0;
        virtual bool IsAdaptiveBalancingEnabled(const std::string& ServiceName) const = 0;
        virtual void SetLoadBalancingWindow(uint32_t WindowSizeMs) = 0;
        virtual uint32_t GetLoadBalancingWindow() const = 0;
        virtual void TuneBalancingParameters(const std::string& ServiceName, const std::unordered_map<std::string, float>& Parameters) = 0;
    };

} // namespace Helianthus::Discovery
