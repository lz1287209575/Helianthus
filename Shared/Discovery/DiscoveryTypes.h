#pragma once

#include "Shared/Common/Types.h"
#include "Shared/Network/NetworkTypes.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Helianthus::Discovery
{
    // Service states
    enum class SERVICE_STATE : uint8_t
    {
        UNKNOWN = 0,
        STARTING = 1,
        HEALTHY = 2,
        UNHEALTHY = 3,
        CRITICAL = 4,
        MAINTENANCE = 5,
        SHUTTING_DOWN = 6,
        OFFLINE = 7
    };

    // Load balancing strategies
    enum class LOAD_BALANCE_STRATEGY : uint8_t
    {
        ROUND_ROBIN = 0,
        WEIGHTED_ROUND_ROBIN = 1,
        LEAST_CONNECTIONS = 2,
        WEIGHTED_LEAST_CONNECTIONS = 3,
        RANDOM = 4,
        WEIGHTED_RANDOM = 5,
        LEAST_RESPONSE_TIME = 6,
        CONSISTENT_HASH = 7,
        IP_HASH = 8
    };

    // Health check types
    enum class HealthCheckType : uint8_t
    {
        TCP_CONNECT = 0,
        HTTP_GET = 1,
        CUSTOM_PROTOCOL = 2,
        HEARTBEAT = 3,
        PING = 4
    };

    // Discovery result codes
    enum class DiscoveryResult : int32_t
    {
        SUCCESS = 0,
        FAILED = -1,
        SERVICE_NOT_FOUND = -2,
        SERVICE_ALREADY_REGISTERED = -3,
        INVALID_SERVICE_INFO = -4,
        REGISTRY_FULL = -5,
        NETWORK_ERROR = -6,
        TIMEOUT = -7,
        AUTHENTICATION_FAILED = -8,
        PERMISSION_DENIED = -9,
        INTERNAL_ERROR = -10
    };

    // Type aliases
    using ServiceInstanceId = uint64_t;
    using ServiceGroupId = uint32_t;
    using HealthScore = uint32_t;
    using LoadWeight = uint32_t;

    static constexpr ServiceInstanceId InvalidServiceInstanceId = 0;
    static constexpr ServiceGroupId InvalidServiceGroupId = 0;
    static constexpr HealthScore MaxHealthScore = 100;
    static constexpr LoadWeight DefaultWeight = 100;

    // Service endpoint information
    struct ServiceEndpoint
    {
        Network::NetworkAddress Address;
        std::string Protocol = "tcp";
        std::unordered_map<std::string, std::string> Metadata;
        
        ServiceEndpoint() = default;
        ServiceEndpoint(const Network::NetworkAddress& Address, const std::string& Protocol = "tcp")
            : Address(Address), Protocol(Protocol) {}
            
        bool IsValid() const { return Address.IsValid() && !Protocol.empty(); }
        std::string ToString() const { return Protocol + "://" + Address.ToString(); }
    };

    // Health check configuration
    struct HealthCheckConfig
    {
        HealthCheckType Type = HealthCheckType::TCP_CONNECT;
        uint32_t IntervalMs = 30000;        // 30 seconds
        uint32_t TimeoutMs = 5000;          // 5 seconds
        uint32_t MaxRetries = 3;
        uint32_t UnhealthyThreshold = 3;    // Consecutive failures to mark unhealthy
        uint32_t HealthyThreshold = 2;      // Consecutive successes to mark healthy
        std::string HealthCheckPath = "/health";
        std::string ExpectedResponse = "OK";
        std::unordered_map<std::string, std::string> CustomHeaders;
    };

    // Load balancing configuration
    struct LoadBalanceConfig
    {
        LOAD_BALANCE_STRATEGY Strategy = LOAD_BALANCE_STRATEGY::ROUND_ROBIN;
        LoadWeight DefaultWeight = DefaultWeight;
        uint32_t MaxConnections = 1000;
        uint32_t ConnectionTimeoutMs = 5000;
        bool EnableStickySession = false;
        std::string StickySessionKey = "session_id";
        uint32_t HashSeed = 12345;
    };

    // Extended service information
    struct ServiceInstance
    {
        ServiceInstanceId InstanceId = InvalidServiceInstanceId;
        Common::ServiceInfo BaseInfo;
        std::vector<ServiceEndpoint> Endpoints;
        SERVICE_STATE State = SERVICE_STATE::UNKNOWN;
        HealthCheckConfig HealthConfig;
        LoadWeight Weight = DefaultWeight;
        HealthScore CurrentHealthScore = 0;
        uint32_t ActiveConnections = 0;
        uint32_t MaxConnections = 1000;
        Common::TimestampMs LastHealthCheck = 0;
        Common::TimestampMs RegisteredTime = 0;
        std::unordered_map<std::string, std::string> Tags;
        std::string Region;
        std::string Zone;
        std::string Environment = "production";
        
        ServiceInstance() = default;
        ServiceInstance(const Common::ServiceInfo& BaseInfo) : BaseInfo(BaseInfo) {}
        
        bool IsHealthy() const 
        { 
            return State == SERVICE_STATE::HEALTHY && CurrentHealthScore > MaxHealthScore / 2; 
        }
        
        bool CanAcceptConnections() const 
        { 
            return IsHealthy() && ActiveConnections < MaxConnections; 
        }
        
        std::string GetPrimaryEndpoint() const
        {
            return !Endpoints.empty() ? Endpoints[0].ToString() : "";
        }
    };

    // Service group for load balancing
    struct ServiceGroup
    {
        ServiceGroupId GroupId = InvalidServiceGroupId;
        std::string ServiceName;
        std::vector<ServiceInstanceId> InstanceIds;
        LoadBalanceConfig LoadBalanceConfig;
        uint32_t TotalWeight = 0;
        uint32_t TotalActiveConnections = 0;
        Common::TimestampMs LastUpdate = 0;
        
        ServiceGroup() = default;
        ServiceGroup(const std::string& ServiceName) : ServiceName(ServiceName) {}
        
        size_t GetHealthyInstanceCount() const { return InstanceIds.size(); } // Simplified
    };

    // Discovery statistics
    struct DiscoveryStats
    {
        uint32_t TotalServices = 0;
        uint32_t HealthyServices = 0;
        uint32_t UnhealthyServices = 0;
        uint32_t TotalServiceInstances = 0;
        uint64_t RegistrationCount = 0;
        uint64_t DeregistrationCount = 0;
        uint64_t DiscoveryRequestCount = 0;
        uint64_t HealthCheckCount = 0;
        uint64_t FailedHealthCheckCount = 0;
        Common::TimestampMs LastUpdate = 0;
    };

    // Registry configuration
    struct RegistryConfig
    {
        uint32_t MaxServices = 10000;
        uint32_t MaxInstancesPerService = 1000;
        uint32_t DefaultTtlMs = 300000;         // 5 minutes
        uint32_t CleanupIntervalMs = 60000;     // 1 minute
        uint32_t HeartbeatTimeoutMs = 90000;    // 1.5 minutes
        bool EnablePersistence = false;
        std::string PersistencePath = "data/registry/";
        bool EnableReplication = false;
        std::vector<Network::NetworkAddress> ReplicaNodes;
    };

    // Forward declarations
    class IServiceRegistry;
    class IServiceDiscovery;
    class ILoadBalancer;
    class IHealthChecker;

    // Smart pointer type definitions
    using ServiceRegistryPtr = std::shared_ptr<IServiceRegistry>;
    using ServiceDiscoveryPtr = std::shared_ptr<IServiceDiscovery>;
    using LoadBalancerPtr = std::shared_ptr<ILoadBalancer>;
    using HealthCheckerPtr = std::shared_ptr<IHealthChecker>;
    using ServiceInstancePtr = std::shared_ptr<ServiceInstance>;
    using ServiceGroupPtr = std::shared_ptr<ServiceGroup>;

    // Callback function types
    using ServiceStateChangeCallback = std::function<void(ServiceInstanceId InstanceId, SERVICE_STATE OldState, SERVICE_STATE NewState)>;
    using ServiceRegistrationCallback = std::function<void(ServiceInstanceId InstanceId, DiscoveryResult Result)>;
    using ServiceDiscoveryCallback = std::function<void(const std::string& ServiceName, const std::vector<ServiceInstancePtr>& Instances)>;
    using HealthCheckCallback = std::function<void(ServiceInstanceId InstanceId, bool IsHealthy, HealthScore Score)>;
    using LoadBalanceCallback = std::function<void(ServiceInstanceId SelectedInstanceId, const std::string& Reason)>;

} // namespace Helianthus::Discovery
