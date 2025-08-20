#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

#include "ILoadBalancer.h"

namespace Helianthus::Discovery
{
/**
 * @brief 高性能负载均衡器实现
 *
 * 支持多种负载均衡策略包括轮询、最少连接数、加权分配、
 * 一致性哈希、健康度感知路由和地理位置优先选择
 */
class LoadBalancer : public ILoadBalancer
{
public:
    LoadBalancer();
    virtual ~LoadBalancer();

    // 禁用拷贝构造和赋值
    LoadBalancer(const LoadBalancer&) = delete;
    LoadBalancer& operator=(const LoadBalancer&) = delete;

    // 移动构造和赋值
    LoadBalancer(LoadBalancer&& Other) noexcept;
    LoadBalancer& operator=(LoadBalancer&& Other) noexcept;

    // ILoadBalancer 实现
    DiscoveryResult Initialize(const LoadBalanceConfig& Config) override;
    void Shutdown() override;
    bool IsInitialized() const override;

    // 服务实例管理
    DiscoveryResult AddServiceInstance(ServiceInstancePtr Instance) override;
    DiscoveryResult RemoveServiceInstance(ServiceInstanceId InstanceId) override;
    DiscoveryResult UpdateServiceInstance(ServiceInstancePtr Instance) override;
    void ClearServiceInstances(const std::string& ServiceName = "") override;
    std::vector<ServiceInstancePtr>
    GetServiceInstances(const std::string& ServiceName) const override;
    uint32_t GetServiceInstanceCount(const std::string& ServiceName) const override;

    // 负载均衡选择
    ServiceInstancePtr SelectInstance(const std::string& ServiceName) override;
    ServiceInstancePtr SelectInstanceWithStrategy(const std::string& ServiceName,
                                                  LoadBalanceStrategy Strategy) override;
    ServiceInstancePtr SelectInstanceWithContext(const std::string& ServiceName,
                                                 const std::string& Context) override;
    ServiceInstancePtr SelectInstanceWithWeight(const std::string& ServiceName,
                                                LoadWeight MinWeight = 0) override;
    ServiceInstancePtr SelectHealthiestInstance(const std::string& ServiceName) override;

    // 策略配置
    void SetLoadBalanceStrategy(const std::string& ServiceName,
                                LoadBalanceStrategy Strategy) override;
    LoadBalanceStrategy GetLoadBalanceStrategy(const std::string& ServiceName) const override;
    void SetDefaultStrategy(LoadBalanceStrategy Strategy) override;
    LoadBalanceStrategy GetDefaultStrategy() const override;

    // 权重管理
    DiscoveryResult SetInstanceWeight(ServiceInstanceId InstanceId, LoadWeight Weight) override;
    LoadWeight GetInstanceWeight(ServiceInstanceId InstanceId) const override;
    void SetDefaultWeight(LoadWeight Weight) override;
    LoadWeight GetDefaultWeight() const override;
    void RebalanceWeights(const std::string& ServiceName) override;

    // 连接跟踪
    DiscoveryResult RecordConnection(ServiceInstanceId InstanceId) override;
    DiscoveryResult RecordDisconnection(ServiceInstanceId InstanceId) override;
    uint32_t GetActiveConnections(ServiceInstanceId InstanceId) const override;
    uint32_t GetTotalActiveConnections(const std::string& ServiceName) const override;
    void ResetConnectionCounts(const std::string& ServiceName = "") override;

    // 健康感知负载均衡
    void UpdateInstanceHealth(ServiceInstanceId InstanceId, HealthScore Score) override;
    HealthScore GetInstanceHealth(ServiceInstanceId InstanceId) const override;
    void SetHealthThreshold(HealthScore MinHealthScore) override;
    HealthScore GetHealthThreshold() const override;
    std::vector<ServiceInstancePtr>
    GetHealthyInstances(const std::string& ServiceName) const override;

    // 响应时间跟踪
    void RecordResponseTime(ServiceInstanceId InstanceId, uint32_t ResponseTimeMs) override;
    uint32_t GetAverageResponseTime(ServiceInstanceId InstanceId) const override;
    ServiceInstancePtr GetFastestInstance(const std::string& ServiceName) override;
    void ResetResponseTimes(const std::string& ServiceName = "") override;

    // 粘性会话
    void EnableStickySession(const std::string& ServiceName,
                             const std::string& SessionKey) override;
    void DisableStickySession(const std::string& ServiceName) override;
    bool IsStickySessionEnabled(const std::string& ServiceName) const override;
    ServiceInstancePtr GetStickyInstance(const std::string& ServiceName,
                                         const std::string& SessionId) override;
    DiscoveryResult BindSession(const std::string& ServiceName,
                                const std::string& SessionId,
                                ServiceInstanceId InstanceId) override;
    void UnbindSession(const std::string& ServiceName, const std::string& SessionId) override;

    // 一致性哈希
    void EnableConsistentHashing(const std::string& ServiceName,
                                 uint32_t VirtualNodes = 150) override;
    void DisableConsistentHashing(const std::string& ServiceName) override;
    bool IsConsistentHashingEnabled(const std::string& ServiceName) const override;
    ServiceInstancePtr GetConsistentHashInstance(const std::string& ServiceName,
                                                 const std::string& Key) override;
    void UpdateHashRing(const std::string& ServiceName) override;

    // 熔断器集成
    void MarkInstanceFailed(ServiceInstanceId InstanceId) override;
    void MarkInstanceRecovered(ServiceInstanceId InstanceId) override;
    bool IsInstanceFailed(ServiceInstanceId InstanceId) const override;
    void SetFailureThreshold(ServiceInstanceId InstanceId, uint32_t Threshold) override;
    void ResetFailureCount(ServiceInstanceId InstanceId) override;

    // 负载指标和统计
    float GetLoadFactor(ServiceInstanceId InstanceId) const override;
    float GetServiceLoadFactor(const std::string& ServiceName) const override;
    std::unordered_map<ServiceInstanceId, uint32_t>
    GetLoadDistribution(const std::string& ServiceName) const override;
    void UpdateLoadMetrics(ServiceInstanceId InstanceId,
                           float CpuUsage,
                           float MemoryUsage,
                           float NetworkUsage) override;

    // 配置管理
    void UpdateConfig(const LoadBalanceConfig& Config) override;
    LoadBalanceConfig GetCurrentConfig() const override;
    void SetMaxConnections(ServiceInstanceId InstanceId, uint32_t MaxConnections) override;
    uint32_t GetMaxConnections(ServiceInstanceId InstanceId) const override;

    // 故障转移和冗余
    void EnableFailover(const std::string& ServiceName, bool Enable = true) override;
    bool IsFailoverEnabled(const std::string& ServiceName) const override;
    void SetFailoverPriority(ServiceInstanceId InstanceId, uint32_t Priority) override;
    ServiceInstancePtr GetFailoverInstance(const std::string& ServiceName) override;

    // 地理位置偏好
    void SetPreferredRegion(const std::string& ServiceName, const std::string& Region) override;
    std::string GetPreferredRegion(const std::string& ServiceName) const override;
    void SetPreferredZone(const std::string& ServiceName, const std::string& Zone) override;
    std::string GetPreferredZone(const std::string& ServiceName) const override;
    ServiceInstancePtr SelectInstanceByLocation(const std::string& ServiceName,
                                                const std::string& Region,
                                                const std::string& Zone) override;

    // 统计和监控
    std::unordered_map<std::string, uint64_t> GetSelectionStats() const override;
    uint64_t GetTotalSelections(const std::string& ServiceName) const override;
    void ResetSelectionStats(const std::string& ServiceName = "") override;
    std::string GetLoadBalancerInfo() const override;

    // 事件回调
    void SetLoadBalanceCallback(LoadBalanceCallback Callback) override;
    void SetInstanceFailureCallback(
        std::function<void(ServiceInstanceId InstanceId, const std::string& Reason)> Callback)
        override;
    void RemoveAllCallbacks() override;

    // 高级功能
    void EnableAdaptiveBalancing(const std::string& ServiceName, bool Enable = true) override;
    bool IsAdaptiveBalancingEnabled(const std::string& ServiceName) const override;
    void SetLoadBalancingWindow(uint32_t WindowSizeMs) override;
    uint32_t GetLoadBalancingWindow() const override;
    void TuneBalancingParameters(const std::string& ServiceName,
                                 const std::unordered_map<std::string, float>& Parameters) override;

private:
    // 内部数据结构
    struct LoadBalanceEntry
    {
        ServiceInstancePtr Instance;
        LoadWeight Weight = 100;
        uint32_t ActiveConnections = 0;
        uint32_t MaxConnections = 1000;
        HealthScore CurrentHealth = MaxHealthScore;
        uint32_t FailureCount = 0;
        uint32_t FailureThreshold = 5;
        bool IsFailed = false;
        uint32_t FailoverPriority = 0;

        // 响应时间统计
        uint32_t TotalResponseTime = 0;
        uint32_t ResponseTimeCount = 0;
        uint32_t LastResponseTime = 0;

        // 负载指标
        float CpuUsage = 0.0f;
        float MemoryUsage = 0.0f;
        float NetworkUsage = 0.0f;

        // 选择统计
        uint64_t SelectionCount = 0;
        Common::TimestampMs LastSelectedTime = 0;
    };

    struct ServiceLoadBalanceInfo
    {
        ServiceLoadBalanceInfo() = default;
        ServiceLoadBalanceInfo(const ServiceLoadBalanceInfo& Other)
            : Strategy(Other.Strategy),
              RoundRobinIndex(Other.RoundRobinIndex.load()),
              Instances(Other.Instances),
              StickySessionEnabled(Other.StickySessionEnabled),
              SessionKey(Other.SessionKey),
              SessionBindings(Other.SessionBindings),
              ConsistentHashingEnabled(Other.ConsistentHashingEnabled),
              VirtualNodes(Other.VirtualNodes),
              HashRing(Other.HashRing),
              FailoverEnabled(Other.FailoverEnabled),
              PreferredRegion(Other.PreferredRegion),
              PreferredZone(Other.PreferredZone),
              AdaptiveBalancingEnabled(Other.AdaptiveBalancingEnabled),
              BalancingParameters(Other.BalancingParameters),
              TotalSelections(Other.TotalSelections)
        {
        }

        ServiceLoadBalanceInfo& operator=(const ServiceLoadBalanceInfo& Other)
        {
            if (this != &Other)
            {
                Strategy = Other.Strategy;
                RoundRobinIndex.store(Other.RoundRobinIndex.load());
                Instances = Other.Instances;
                StickySessionEnabled = Other.StickySessionEnabled;
                SessionKey = Other.SessionKey;
                SessionBindings = Other.SessionBindings;
                ConsistentHashingEnabled = Other.ConsistentHashingEnabled;
                VirtualNodes = Other.VirtualNodes;
                HashRing = Other.HashRing;
                FailoverEnabled = Other.FailoverEnabled;
                PreferredRegion = Other.PreferredRegion;
                PreferredZone = Other.PreferredZone;
                AdaptiveBalancingEnabled = Other.AdaptiveBalancingEnabled;
                BalancingParameters = Other.BalancingParameters;
                TotalSelections = Other.TotalSelections;
            }
            return *this;
        }

        ServiceLoadBalanceInfo(ServiceLoadBalanceInfo&& Other) noexcept
            : Strategy(Other.Strategy),
              RoundRobinIndex(Other.RoundRobinIndex.load()),
              Instances(std::move(Other.Instances)),
              StickySessionEnabled(Other.StickySessionEnabled),
              SessionKey(std::move(Other.SessionKey)),
              SessionBindings(std::move(Other.SessionBindings)),
              ConsistentHashingEnabled(Other.ConsistentHashingEnabled),
              VirtualNodes(Other.VirtualNodes),
              HashRing(std::move(Other.HashRing)),
              FailoverEnabled(Other.FailoverEnabled),
              PreferredRegion(std::move(Other.PreferredRegion)),
              PreferredZone(std::move(Other.PreferredZone)),
              AdaptiveBalancingEnabled(Other.AdaptiveBalancingEnabled),
              BalancingParameters(std::move(Other.BalancingParameters)),
              TotalSelections(Other.TotalSelections)
        {
        }

        ServiceLoadBalanceInfo& operator=(ServiceLoadBalanceInfo&& Other) noexcept
        {
            if (this != &Other)
            {
                Strategy = Other.Strategy;
                RoundRobinIndex.store(Other.RoundRobinIndex.load());
                Instances = std::move(Other.Instances);
                StickySessionEnabled = Other.StickySessionEnabled;
                SessionKey = std::move(Other.SessionKey);
                SessionBindings = std::move(Other.SessionBindings);
                ConsistentHashingEnabled = Other.ConsistentHashingEnabled;
                VirtualNodes = Other.VirtualNodes;
                HashRing = std::move(Other.HashRing);
                FailoverEnabled = Other.FailoverEnabled;
                PreferredRegion = std::move(Other.PreferredRegion);
                PreferredZone = std::move(Other.PreferredZone);
                AdaptiveBalancingEnabled = Other.AdaptiveBalancingEnabled;
                BalancingParameters = std::move(Other.BalancingParameters);
                TotalSelections = Other.TotalSelections;
            }
            return *this;
        }

        LoadBalanceStrategy Strategy = LoadBalanceStrategy::ROUND_ROBIN;
        std::atomic<uint32_t> RoundRobinIndex{0};
        std::unordered_map<ServiceInstanceId, LoadBalanceEntry> Instances;

        // 粘性会话
        bool StickySessionEnabled = false;
        std::string SessionKey;
        std::unordered_map<std::string, ServiceInstanceId> SessionBindings;

        // 一致性哈希
        bool ConsistentHashingEnabled = false;
        uint32_t VirtualNodes = 150;
        std::map<uint32_t, ServiceInstanceId> HashRing;

        // 故障转移
        bool FailoverEnabled = false;

        // 地理位置偏好
        std::string PreferredRegion;
        std::string PreferredZone;

        // 自适应均衡
        bool AdaptiveBalancingEnabled = false;
        std::unordered_map<std::string, float> BalancingParameters;

        // 统计信息
        uint64_t TotalSelections = 0;
    };

    // 选择算法实现
    ServiceInstancePtr SelectRoundRobin(const std::string& ServiceName);
    ServiceInstancePtr SelectLeastConnections(const std::string& ServiceName);
    ServiceInstancePtr SelectWeightedRoundRobin(const std::string& ServiceName);
    ServiceInstancePtr SelectWeightedRandom(const std::string& ServiceName);
    ServiceInstancePtr SelectFastestResponse(const std::string& ServiceName);
    ServiceInstancePtr SelectRandomInstance(const std::string& ServiceName);
    ServiceInstancePtr SelectByHash(const std::string& ServiceName, const std::string& Key);

    // 辅助方法
    ServiceInstancePtr SelectHealthyInstance(const std::string& ServiceName,
                                             LoadBalanceStrategy Strategy);
    bool IsInstanceHealthy(const LoadBalanceEntry& Entry) const;
    bool IsInstanceAvailable(const LoadBalanceEntry& Entry) const;
    std::vector<ServiceInstanceId> GetAvailableInstances(const std::string& ServiceName) const;
    float CalculateLoadFactor(const LoadBalanceEntry& Entry) const;
    uint32_t CalculateInstanceHash(ServiceInstanceId InstanceId) const;
    uint32_t CalculateStringHash(const std::string& Str) const;
    void UpdateSelectionStats(const std::string& ServiceName, ServiceInstanceId InstanceId);
    void NotifyInstanceFailure(ServiceInstanceId InstanceId, const std::string& Reason);

private:
    // 配置和状态
    LoadBalanceConfig Config;
    std::atomic<bool> InitializedFlag{false};
    std::atomic<bool> ShuttingDownFlag{false};

    // 默认设置
    std::atomic<LoadBalanceStrategy> DefaultStrategy{LoadBalanceStrategy::ROUND_ROBIN};
    std::atomic<LoadWeight> DefaultWeight{100};
    std::atomic<HealthScore> HealthThreshold{50};
    std::atomic<uint32_t> LoadBalancingWindow{30000};  // 30秒窗口

    // 服务实例存储
    mutable std::mutex ServicesMutex;
    std::unordered_map<std::string, ServiceLoadBalanceInfo> Services;
    std::unordered_map<ServiceInstanceId, std::string> InstanceToService;  // 反向查找

    // 随机数生成器
    mutable std::mutex RandomMutex;
    std::random_device RandomDevice;
    std::mt19937 RandomGenerator;

    // 回调函数
    LoadBalanceCallback LoadBalanceCallbackFunc;
    std::function<void(ServiceInstanceId, const std::string&)> InstanceFailureCallback;
};

}  // namespace Helianthus::Discovery