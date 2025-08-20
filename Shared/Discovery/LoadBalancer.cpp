#include "LoadBalancer.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>

namespace Helianthus::Discovery
{
LoadBalancer::LoadBalancer() : RandomGenerator(RandomDevice())
{
    std::cout << "[LoadBalancer] Load balancer created" << std::endl;
}

LoadBalancer::~LoadBalancer()
{
    Shutdown();
}

LoadBalancer::LoadBalancer(LoadBalancer&& Other) noexcept
    : Config(std::move(Other.Config)),
      InitializedFlag(Other.InitializedFlag.load()),
      ShuttingDownFlag(Other.ShuttingDownFlag.load()),
      DefaultStrategy(Other.DefaultStrategy.load()),
      DefaultWeight(Other.DefaultWeight.load()),
      HealthThreshold(Other.HealthThreshold.load()),
      LoadBalancingWindow(Other.LoadBalancingWindow.load()),
      Services(std::move(Other.Services)),
      InstanceToService(std::move(Other.InstanceToService)),
      RandomGenerator(RandomDevice()),
      LoadBalanceCallbackFunc(std::move(Other.LoadBalanceCallbackFunc)),
      InstanceFailureCallback(std::move(Other.InstanceFailureCallback))
{
    Other.InitializedFlag = false;
    Other.ShuttingDownFlag = false;
}

LoadBalancer& LoadBalancer::operator=(LoadBalancer&& Other) noexcept
{
    if (this != &Other)
    {
        Shutdown();

        Config = std::move(Other.Config);
        InitializedFlag = Other.InitializedFlag.load();
        ShuttingDownFlag = Other.ShuttingDownFlag.load();
        DefaultStrategy = Other.DefaultStrategy.load();
        DefaultWeight = Other.DefaultWeight.load();
        HealthThreshold = Other.HealthThreshold.load();
        LoadBalancingWindow = Other.LoadBalancingWindow.load();
        Services = std::move(Other.Services);
        InstanceToService = std::move(Other.InstanceToService);
        LoadBalanceCallbackFunc = std::move(Other.LoadBalanceCallbackFunc);
        InstanceFailureCallback = std::move(Other.InstanceFailureCallback);

        Other.InitializedFlag = false;
        Other.ShuttingDownFlag = false;
    }
    return *this;
}

DiscoveryResult LoadBalancer::Initialize(const LoadBalanceConfig& Config)
{
    if (InitializedFlag)
    {
        return DiscoveryResult::INTERNAL_ERROR;
    }

    this->Config = Config;
    DefaultStrategy = Config.DefaultStrategy;
    DefaultWeight = Config.DefaultWeight;
    HealthThreshold = Config.MinHealthScore;

    InitializedFlag = true;
    ShuttingDownFlag = false;

    std::cout << "[LoadBalancer] 初始化完成，默认策略: " << static_cast<int>(DefaultStrategy.load())
              << std::endl;

    return DiscoveryResult::SUCCESS;
}

void LoadBalancer::Shutdown()
{
    if (!InitializedFlag)
    {
        return;
    }

    std::cout << "[LoadBalancer] Starting shutdown" << std::endl;

    ShuttingDownFlag = true;

    {
        std::lock_guard<std::mutex> Lock(ServicesMutex);
        Services.clear();
        InstanceToService.clear();
    }

    RemoveAllCallbacks();
    InitializedFlag = false;

    std::cout << "[LoadBalancer] 关闭完成" << std::endl;
}

bool LoadBalancer::IsInitialized() const
{
    return InitializedFlag;
}

DiscoveryResult LoadBalancer::AddServiceInstance(ServiceInstancePtr Instance)
{
    if (!InitializedFlag || !Instance)
    {
        return DiscoveryResult::INVALID_SERVICE_INFO;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    const std::string& ServiceName = Instance->BaseInfo.ServiceName;
    ServiceInstanceId InstanceId = Instance->InstanceId;

    // 确保服务信息存在
    if (Services.find(ServiceName) == Services.end())
    {
        Services.emplace(ServiceName, ServiceLoadBalanceInfo{});
        Services[ServiceName].Strategy = DefaultStrategy;
    }

    // 添加实例条目
    LoadBalanceEntry Entry;
    Entry.Instance = Instance;
    Entry.Weight = DefaultWeight.load();
    Entry.CurrentHealth = Instance->CurrentHealthScore;
    Entry.ActiveConnections = Instance->ActiveConnections;

    Services[ServiceName].Instances[InstanceId] = Entry;
    InstanceToService[InstanceId] = ServiceName;

    // 如果启用了一致性哈希，更新哈希环
    if (Services[ServiceName].ConsistentHashingEnabled)
    {
        UpdateHashRing(ServiceName);
    }

    std::cout << "[LoadBalancer] 添加服务实例: " << ServiceName << ", 实例ID: " << InstanceId
              << std::endl;

    return DiscoveryResult::SUCCESS;
}

DiscoveryResult LoadBalancer::RemoveServiceInstance(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    const std::string& ServiceName = ServiceIt->second;
    auto& ServiceInfo = Services[ServiceName];

    // 移除实例
    ServiceInfo.Instances.erase(InstanceId);
    InstanceToService.erase(ServiceIt);

    // 清理会话绑定
    if (ServiceInfo.StickySessionEnabled)
    {
        for (auto It = ServiceInfo.SessionBindings.begin();
             It != ServiceInfo.SessionBindings.end();)
        {
            if (It->second == InstanceId)
            {
                It = ServiceInfo.SessionBindings.erase(It);
            }
            else
            {
                ++It;
            }
        }
    }

    // 如果启用了一致性哈希，更新哈希环
    if (ServiceInfo.ConsistentHashingEnabled)
    {
        UpdateHashRing(ServiceName);
    }

    // 如果服务没有实例了，移除服务信息
    if (ServiceInfo.Instances.empty())
    {
        Services.erase(ServiceName);
    }

    std::cout << "[LoadBalancer] 移除服务实例: " << ServiceName << ", 实例ID: " << InstanceId
              << std::endl;

    return DiscoveryResult::SUCCESS;
}

DiscoveryResult LoadBalancer::UpdateServiceInstance(ServiceInstancePtr Instance)
{
    if (!Instance)
    {
        return DiscoveryResult::INVALID_SERVICE_INFO;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    ServiceInstanceId InstanceId = Instance->InstanceId;
    auto ServiceIt = InstanceToService.find(InstanceId);

    if (ServiceIt == InstanceToService.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    const std::string& ServiceName = ServiceIt->second;
    auto& ServiceInfo = Services[ServiceName];
    auto& Entry = ServiceInfo.Instances[InstanceId];

    // 更新实例信息
    Entry.Instance = Instance;
    Entry.CurrentHealth = Instance->CurrentHealthScore;
    Entry.ActiveConnections = Instance->ActiveConnections;

    return DiscoveryResult::SUCCESS;
}

void LoadBalancer::ClearServiceInstances(const std::string& ServiceName)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    if (ServiceName.empty())
    {
        // 清空所有服务
        Services.clear();
        InstanceToService.clear();
    }
    else
    {
        // 清空指定服务
        auto It = Services.find(ServiceName);
        if (It != Services.end())
        {
            // 移除反向映射
            for (const auto& Instance : It->second.Instances)
            {
                InstanceToService.erase(Instance.first);
            }
            Services.erase(It);
        }
    }
}

std::vector<ServiceInstancePtr>
LoadBalancer::GetServiceInstances(const std::string& ServiceName) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    std::vector<ServiceInstancePtr> Result;
    const auto It = Services.find(ServiceName);

    if (It != Services.end())
    {
        for (const auto& Entry : It->second.Instances)
        {
            Result.push_back(Entry.second.Instance);
        }
    }

    return Result;
}

uint32_t LoadBalancer::GetServiceInstanceCount(const std::string& ServiceName) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    const auto It = Services.find(ServiceName);
    return It != Services.end() ? static_cast<uint32_t>(It->second.Instances.size()) : 0;
}

ServiceInstancePtr LoadBalancer::SelectInstance(const std::string& ServiceName)
{
    if (!InitializedFlag)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto It = Services.find(ServiceName);
    if (It == Services.end() || It->second.Instances.empty())
    {
        return nullptr;
    }

    return SelectHealthyInstance(ServiceName, It->second.Strategy);
}

ServiceInstancePtr LoadBalancer::SelectInstanceWithStrategy(const std::string& ServiceName,
                                                            LoadBalanceStrategy Strategy)
{
    if (!InitializedFlag)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto It = Services.find(ServiceName);
    if (It == Services.end() || It->second.Instances.empty())
    {
        return nullptr;
    }

    return SelectHealthyInstance(ServiceName, Strategy);
}

ServiceInstancePtr LoadBalancer::SelectInstanceWithContext(const std::string& ServiceName,
                                                           const std::string& Context)
{
    if (!InitializedFlag)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto It = Services.find(ServiceName);
    if (It == Services.end() || It->second.Instances.empty())
    {
        return nullptr;
    }

    // 如果启用了粘性会话，尝试获取绑定的实例
    if (It->second.StickySessionEnabled)
    {
        auto SessionInstance = GetStickyInstance(ServiceName, Context);
        if (SessionInstance)
        {
            return SessionInstance;
        }
    }

    // 如果启用了一致性哈希，使用哈希选择
    if (It->second.ConsistentHashingEnabled)
    {
        return GetConsistentHashInstance(ServiceName, Context);
    }

    // 否则使用默认策略
    return SelectHealthyInstance(ServiceName, It->second.Strategy);
}

ServiceInstancePtr LoadBalancer::SelectInstanceWithWeight(const std::string& ServiceName,
                                                          LoadWeight MinWeight)
{
    if (!InitializedFlag)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto It = Services.find(ServiceName);
    if (It == Services.end() || It->second.Instances.empty())
    {
        return nullptr;
    }

    // 筛选满足最小权重要求的实例
    std::vector<ServiceInstanceId> ValidInstances;
    for (const auto& Instance : It->second.Instances)
    {
        if (Instance.second.Weight >= MinWeight && IsInstanceAvailable(Instance.second))
        {
            ValidInstances.push_back(Instance.first);
        }
    }

    if (ValidInstances.empty())
    {
        return nullptr;
    }

    // 使用加权随机选择
    return SelectWeightedRandom(ServiceName);
}

ServiceInstancePtr LoadBalancer::SelectHealthiestInstance(const std::string& ServiceName)
{
    if (!InitializedFlag)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto It = Services.find(ServiceName);
    if (It == Services.end() || It->second.Instances.empty())
    {
        return nullptr;
    }

    ServiceInstancePtr BestInstance = nullptr;
    HealthScore BestScore = 0;

    for (const auto& Instance : It->second.Instances)
    {
        if (IsInstanceAvailable(Instance.second) && Instance.second.CurrentHealth > BestScore)
        {
            BestScore = Instance.second.CurrentHealth;
            BestInstance = Instance.second.Instance;
        }
    }

    if (BestInstance)
    {
        UpdateSelectionStats(ServiceName, BestInstance->InstanceId);
    }

    return BestInstance;
}

// 策略选择实现
ServiceInstancePtr LoadBalancer::SelectRoundRobin(const std::string& ServiceName)
{
    auto& ServiceInfo = Services[ServiceName];
    auto AvailableInstances = GetAvailableInstances(ServiceName);

    if (AvailableInstances.empty())
    {
        return nullptr;
    }

    uint32_t Index = ServiceInfo.RoundRobinIndex.fetch_add(1, std::memory_order_relaxed) %
                     AvailableInstances.size();
    ServiceInstanceId SelectedId = AvailableInstances[Index];

    UpdateSelectionStats(ServiceName, SelectedId);
    return ServiceInfo.Instances[SelectedId].Instance;
}

ServiceInstancePtr LoadBalancer::SelectLeastConnections(const std::string& ServiceName)
{
    auto& ServiceInfo = Services[ServiceName];
    ServiceInstancePtr BestInstance = nullptr;
    uint32_t MinConnections = UINT32_MAX;

    for (const auto& Instance : ServiceInfo.Instances)
    {
        if (IsInstanceAvailable(Instance.second) &&
            Instance.second.ActiveConnections < MinConnections)
        {
            MinConnections = Instance.second.ActiveConnections;
            BestInstance = Instance.second.Instance;
        }
    }

    if (BestInstance)
    {
        UpdateSelectionStats(ServiceName, BestInstance->InstanceId);
    }

    return BestInstance;
}

ServiceInstancePtr LoadBalancer::SelectWeightedRoundRobin(const std::string& ServiceName)
{
    auto& ServiceInfo = Services[ServiceName];

    // 简化的加权轮询 - 根据权重重复添加实例到候选列表
    std::vector<ServiceInstanceId> WeightedInstances;

    for (const auto& Instance : ServiceInfo.Instances)
    {
        if (IsInstanceAvailable(Instance.second))
        {
            uint32_t Weight = std::max(1u, static_cast<uint32_t>(Instance.second.Weight));
            for (uint32_t i = 0; i < Weight; ++i)
            {
                WeightedInstances.push_back(Instance.first);
            }
        }
    }

    if (WeightedInstances.empty())
    {
        return nullptr;
    }

    uint32_t Index = ServiceInfo.RoundRobinIndex.fetch_add(1, std::memory_order_relaxed) %
                     WeightedInstances.size();
    ServiceInstanceId SelectedId = WeightedInstances[Index];

    UpdateSelectionStats(ServiceName, SelectedId);
    return ServiceInfo.Instances[SelectedId].Instance;
}

ServiceInstancePtr LoadBalancer::SelectWeightedRandom(const std::string& ServiceName)
{
    auto& ServiceInfo = Services[ServiceName];

    // 计算总权重
    LoadWeight TotalWeight = 0;
    std::vector<std::pair<ServiceInstanceId, LoadWeight>> ValidInstances;

    for (const auto& Instance : ServiceInfo.Instances)
    {
        if (IsInstanceAvailable(Instance.second))
        {
            TotalWeight += Instance.second.Weight;
            ValidInstances.push_back({Instance.first, Instance.second.Weight});
        }
    }

    if (ValidInstances.empty() || TotalWeight == 0)
    {
        return nullptr;
    }

    // 加权随机选择
    std::lock_guard<std::mutex> RandomLock(RandomMutex);
    std::uniform_int_distribution<LoadWeight> Distribution(1, TotalWeight);
    LoadWeight RandomValue = Distribution(RandomGenerator);

    LoadWeight CurrentWeight = 0;
    for (const auto& Instance : ValidInstances)
    {
        CurrentWeight += Instance.second;
        if (RandomValue <= CurrentWeight)
        {
            UpdateSelectionStats(ServiceName, Instance.first);
            return ServiceInfo.Instances[Instance.first].Instance;
        }
    }

    return nullptr;
}

ServiceInstancePtr LoadBalancer::SelectFastestResponse(const std::string& ServiceName)
{
    auto& ServiceInfo = Services[ServiceName];
    ServiceInstancePtr BestInstance = nullptr;
    uint32_t BestResponseTime = UINT32_MAX;

    for (const auto& Instance : ServiceInfo.Instances)
    {
        if (IsInstanceAvailable(Instance.second))
        {
            uint32_t AvgResponseTime =
                Instance.second.ResponseTimeCount > 0
                    ? Instance.second.TotalResponseTime / Instance.second.ResponseTimeCount
                    : 0;

            if (AvgResponseTime < BestResponseTime && AvgResponseTime > 0)
            {
                BestResponseTime = AvgResponseTime;
                BestInstance = Instance.second.Instance;
            }
        }
    }

    if (BestInstance)
    {
        UpdateSelectionStats(ServiceName, BestInstance->InstanceId);
    }

    return BestInstance;
}

ServiceInstancePtr LoadBalancer::SelectRandomInstance(const std::string& ServiceName)
{
    auto AvailableInstances = GetAvailableInstances(ServiceName);

    if (AvailableInstances.empty())
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> RandomLock(RandomMutex);
    std::uniform_int_distribution<size_t> Distribution(0, AvailableInstances.size() - 1);
    size_t Index = Distribution(RandomGenerator);

    ServiceInstanceId SelectedId = AvailableInstances[Index];
    UpdateSelectionStats(ServiceName, SelectedId);

    return Services[ServiceName].Instances[SelectedId].Instance;
}

// 辅助方法实现
ServiceInstancePtr LoadBalancer::SelectHealthyInstance(const std::string& ServiceName,
                                                       LoadBalanceStrategy Strategy)
{
    switch (Strategy)
    {
        case LoadBalanceStrategy::ROUND_ROBIN:
            return SelectRoundRobin(ServiceName);
        case LoadBalanceStrategy::LEAST_CONNECTIONS:
            return SelectLeastConnections(ServiceName);
        case LoadBalanceStrategy::WEIGHTED_ROUND_ROBIN:
            return SelectWeightedRoundRobin(ServiceName);
        case LoadBalanceStrategy::WEIGHTED_RANDOM:
            return SelectWeightedRandom(ServiceName);
        case LoadBalanceStrategy::LEAST_RESPONSE_TIME:
            return SelectFastestResponse(ServiceName);
        case LoadBalanceStrategy::RANDOM:
            return SelectRandomInstance(ServiceName);
        default:
            return SelectRoundRobin(ServiceName);
    }
}

bool LoadBalancer::IsInstanceHealthy(const LoadBalanceEntry& Entry) const
{
    return Entry.CurrentHealth >= HealthThreshold.load();
}

bool LoadBalancer::IsInstanceAvailable(const LoadBalanceEntry& Entry) const
{
    return !Entry.IsFailed && IsInstanceHealthy(Entry) &&
           Entry.ActiveConnections < Entry.MaxConnections;
}

std::vector<ServiceInstanceId>
LoadBalancer::GetAvailableInstances(const std::string& ServiceName) const
{
    std::vector<ServiceInstanceId> Result;

    auto It = Services.find(ServiceName);
    if (It != Services.end())
    {
        for (const auto& Instance : It->second.Instances)
        {
            if (IsInstanceAvailable(Instance.second))
            {
                Result.push_back(Instance.first);
            }
        }
    }

    return Result;
}

void LoadBalancer::UpdateSelectionStats(const std::string& ServiceName,
                                        ServiceInstanceId InstanceId)
{
    auto& ServiceInfo = Services[ServiceName];
    auto& Instance = ServiceInfo.Instances[InstanceId];

    Instance.SelectionCount++;
    Instance.LastSelectedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
    ServiceInfo.TotalSelections++;

    // 调用回调
    if (LoadBalanceCallbackFunc)
    {
        LoadBalanceCallbackFunc(InstanceId, ServiceName);
    }
}

// 简化实现其他必需的方法
void LoadBalancer::SetLoadBalanceStrategy(const std::string& ServiceName,
                                          LoadBalanceStrategy Strategy)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);
    if (Services.find(ServiceName) == Services.end())
    {
        Services.emplace(ServiceName, ServiceLoadBalanceInfo{});
    }
    Services[ServiceName].Strategy = Strategy;
}

LoadBalanceStrategy LoadBalancer::GetLoadBalanceStrategy(const std::string& ServiceName) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);
    const auto It = Services.find(ServiceName);
    return It != Services.end() ? It->second.Strategy : DefaultStrategy.load();
}

void LoadBalancer::SetDefaultStrategy(LoadBalanceStrategy Strategy)
{
    DefaultStrategy = Strategy;
}

LoadBalanceStrategy LoadBalancer::GetDefaultStrategy() const
{
    return DefaultStrategy.load();
}

DiscoveryResult LoadBalancer::SetInstanceWeight(ServiceInstanceId InstanceId, LoadWeight Weight)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    auto& Instance = Services[ServiceIt->second].Instances[InstanceId];
    Instance.Weight = Weight;

    return DiscoveryResult::SUCCESS;
}

LoadWeight LoadBalancer::GetInstanceWeight(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    const auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return DefaultWeight.load();
    }

    const auto& ServiceInfo = Services.at(ServiceIt->second);
    const auto InstanceIt = ServiceInfo.Instances.find(InstanceId);

    return InstanceIt != ServiceInfo.Instances.end() ? InstanceIt->second.Weight
                                                     : DefaultWeight.load();
}

void LoadBalancer::SetDefaultWeight(LoadWeight Weight)
{
    DefaultWeight = Weight;
}

LoadWeight LoadBalancer::GetDefaultWeight() const
{
    return DefaultWeight.load();
}

DiscoveryResult LoadBalancer::RecordConnection(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    auto& Instance = Services[ServiceIt->second].Instances[InstanceId];
    Instance.ActiveConnections++;

    return DiscoveryResult::SUCCESS;
}

DiscoveryResult LoadBalancer::RecordDisconnection(ServiceInstanceId InstanceId)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return DiscoveryResult::SERVICE_NOT_FOUND;
    }

    auto& Instance = Services[ServiceIt->second].Instances[InstanceId];
    if (Instance.ActiveConnections > 0)
    {
        Instance.ActiveConnections--;
    }

    return DiscoveryResult::SUCCESS;
}

uint32_t LoadBalancer::GetActiveConnections(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    const auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return 0;
    }

    const auto& ServiceInfo = Services.at(ServiceIt->second);
    const auto InstanceIt = ServiceInfo.Instances.find(InstanceId);

    return InstanceIt != ServiceInfo.Instances.end() ? InstanceIt->second.ActiveConnections : 0;
}

void LoadBalancer::UpdateInstanceHealth(ServiceInstanceId InstanceId, HealthScore Score)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt != InstanceToService.end())
    {
        auto& Instance = Services[ServiceIt->second].Instances[InstanceId];
        Instance.CurrentHealth = Score;
    }
}

HealthScore LoadBalancer::GetInstanceHealth(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    const auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return 0;
    }

    const auto& ServiceInfo = Services.at(ServiceIt->second);
    const auto InstanceIt = ServiceInfo.Instances.find(InstanceId);

    return InstanceIt != ServiceInfo.Instances.end() ? InstanceIt->second.CurrentHealth : 0;
}

void LoadBalancer::SetHealthThreshold(HealthScore MinHealthScore)
{
    HealthThreshold = MinHealthScore;
}

HealthScore LoadBalancer::GetHealthThreshold() const
{
    return HealthThreshold.load();
}

std::vector<ServiceInstancePtr>
LoadBalancer::GetHealthyInstances(const std::string& ServiceName) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    std::vector<ServiceInstancePtr> Result;
    const auto It = Services.find(ServiceName);

    if (It != Services.end())
    {
        for (const auto& Instance : It->second.Instances)
        {
            if (IsInstanceHealthy(Instance.second))
            {
                Result.push_back(Instance.second.Instance);
            }
        }
    }

    return Result;
}

void LoadBalancer::RecordResponseTime(ServiceInstanceId InstanceId, uint32_t ResponseTimeMs)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt != InstanceToService.end())
    {
        auto& Instance = Services[ServiceIt->second].Instances[InstanceId];
        Instance.TotalResponseTime += ResponseTimeMs;
        Instance.ResponseTimeCount++;
        Instance.LastResponseTime = ResponseTimeMs;
    }
}

uint32_t LoadBalancer::GetAverageResponseTime(ServiceInstanceId InstanceId) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);

    const auto ServiceIt = InstanceToService.find(InstanceId);
    if (ServiceIt == InstanceToService.end())
    {
        return 0;
    }

    const auto& ServiceInfo = Services.at(ServiceIt->second);
    const auto InstanceIt = ServiceInfo.Instances.find(InstanceId);

    if (InstanceIt != ServiceInfo.Instances.end() && InstanceIt->second.ResponseTimeCount > 0)
    {
        return InstanceIt->second.TotalResponseTime / InstanceIt->second.ResponseTimeCount;
    }

    return 0;
}

ServiceInstancePtr LoadBalancer::GetFastestInstance(const std::string& ServiceName)
{
    return SelectInstanceWithStrategy(ServiceName, LoadBalanceStrategy::LEAST_RESPONSE_TIME);
}

// 简化实现其他方法（占位符实现）
void LoadBalancer::RebalanceWeights(const std::string& ServiceName) {}
uint32_t LoadBalancer::GetTotalActiveConnections(const std::string& ServiceName) const
{
    return 0;
}
void LoadBalancer::ResetConnectionCounts(const std::string& ServiceName) {}
void LoadBalancer::ResetResponseTimes(const std::string& ServiceName) {}
void LoadBalancer::EnableStickySession(const std::string& ServiceName,
                                       const std::string& SessionKey)
{
}
void LoadBalancer::DisableStickySession(const std::string& ServiceName) {}
bool LoadBalancer::IsStickySessionEnabled(const std::string& ServiceName) const
{
    return false;
}
ServiceInstancePtr LoadBalancer::GetStickyInstance(const std::string& ServiceName,
                                                   const std::string& SessionId)
{
    return nullptr;
}
DiscoveryResult LoadBalancer::BindSession(const std::string& ServiceName,
                                          const std::string& SessionId,
                                          ServiceInstanceId InstanceId)
{
    return DiscoveryResult::SUCCESS;
}
void LoadBalancer::UnbindSession(const std::string& ServiceName, const std::string& SessionId) {}
void LoadBalancer::EnableConsistentHashing(const std::string& ServiceName, uint32_t VirtualNodes) {}
void LoadBalancer::DisableConsistentHashing(const std::string& ServiceName) {}
bool LoadBalancer::IsConsistentHashingEnabled(const std::string& ServiceName) const
{
    return false;
}
ServiceInstancePtr LoadBalancer::GetConsistentHashInstance(const std::string& ServiceName,
                                                           const std::string& Key)
{
    return nullptr;
}
void LoadBalancer::UpdateHashRing(const std::string& ServiceName) {}
void LoadBalancer::MarkInstanceFailed(ServiceInstanceId InstanceId) {}
void LoadBalancer::MarkInstanceRecovered(ServiceInstanceId InstanceId) {}
bool LoadBalancer::IsInstanceFailed(ServiceInstanceId InstanceId) const
{
    return false;
}
void LoadBalancer::SetFailureThreshold(ServiceInstanceId InstanceId, uint32_t Threshold) {}
void LoadBalancer::ResetFailureCount(ServiceInstanceId InstanceId) {}
float LoadBalancer::GetLoadFactor(ServiceInstanceId InstanceId) const
{
    return 0.0f;
}
float LoadBalancer::GetServiceLoadFactor(const std::string& ServiceName) const
{
    return 0.0f;
}
std::unordered_map<ServiceInstanceId, uint32_t>
LoadBalancer::GetLoadDistribution(const std::string& ServiceName) const
{
    return {};
}
void LoadBalancer::UpdateLoadMetrics(ServiceInstanceId InstanceId,
                                     float CpuUsage,
                                     float MemoryUsage,
                                     float NetworkUsage)
{
}
void LoadBalancer::UpdateConfig(const LoadBalanceConfig& Config)
{
    this->Config = Config;
}
LoadBalanceConfig LoadBalancer::GetCurrentConfig() const
{
    return Config;
}
void LoadBalancer::SetMaxConnections(ServiceInstanceId InstanceId, uint32_t MaxConnections) {}
uint32_t LoadBalancer::GetMaxConnections(ServiceInstanceId InstanceId) const
{
    return 1000;
}
void LoadBalancer::EnableFailover(const std::string& ServiceName, bool Enable) {}
bool LoadBalancer::IsFailoverEnabled(const std::string& ServiceName) const
{
    return false;
}
void LoadBalancer::SetFailoverPriority(ServiceInstanceId InstanceId, uint32_t Priority) {}
ServiceInstancePtr LoadBalancer::GetFailoverInstance(const std::string& ServiceName)
{
    return nullptr;
}
void LoadBalancer::SetPreferredRegion(const std::string& ServiceName, const std::string& Region) {}
std::string LoadBalancer::GetPreferredRegion(const std::string& ServiceName) const
{
    return "";
}
void LoadBalancer::SetPreferredZone(const std::string& ServiceName, const std::string& Zone) {}
std::string LoadBalancer::GetPreferredZone(const std::string& ServiceName) const
{
    return "";
}
ServiceInstancePtr LoadBalancer::SelectInstanceByLocation(const std::string& ServiceName,
                                                          const std::string& Region,
                                                          const std::string& Zone)
{
    return nullptr;
}
std::unordered_map<std::string, uint64_t> LoadBalancer::GetSelectionStats() const
{
    return {};
}
uint64_t LoadBalancer::GetTotalSelections(const std::string& ServiceName) const
{
    return 0;
}
void LoadBalancer::ResetSelectionStats(const std::string& ServiceName) {}
std::string LoadBalancer::GetLoadBalancerInfo() const
{
    return "LoadBalancer";
}
void LoadBalancer::SetLoadBalanceCallback(LoadBalanceCallback Callback)
{
    LoadBalanceCallbackFunc = std::move(Callback);
}
void LoadBalancer::SetInstanceFailureCallback(
    std::function<void(ServiceInstanceId, const std::string&)> Callback)
{
    InstanceFailureCallback = std::move(Callback);
}
void LoadBalancer::RemoveAllCallbacks()
{
    LoadBalanceCallbackFunc = nullptr;
    InstanceFailureCallback = nullptr;
}
void LoadBalancer::EnableAdaptiveBalancing(const std::string& ServiceName, bool Enable) {}
bool LoadBalancer::IsAdaptiveBalancingEnabled(const std::string& ServiceName) const
{
    return false;
}
void LoadBalancer::SetLoadBalancingWindow(uint32_t WindowSizeMs)
{
    LoadBalancingWindow = WindowSizeMs;
}
uint32_t LoadBalancer::GetLoadBalancingWindow() const
{
    return LoadBalancingWindow.load();
}
void LoadBalancer::TuneBalancingParameters(const std::string& ServiceName,
                                           const std::unordered_map<std::string, float>& Parameters)
{
}

}  // namespace Helianthus::Discovery