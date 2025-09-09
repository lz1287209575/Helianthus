#include "ConnectionManager.h"

#include <chrono>
#include <algorithm>

namespace Helianthus::Network
{

ConnectionManager::ConnectionManager()
{
    // Initialize with default config
    Config.MaxConnections = 1000;
    Config.ConnectionTimeoutMs = 5000;
    Config.EnableKeepalive = true;
    Config.KeepAliveIntervalMs = 30000;
}

ConnectionManager::~ConnectionManager()
{
    Shutdown();
}

NetworkError ConnectionManager::Initialize(const NetworkConfig& NetworkConfig)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    if (Initialized.load())
    {
        return NetworkError::ALREADY_INITIALIZED;
    }
    
    Config = NetworkConfig;
    Initialized.store(true);
    
    return NetworkError::SUCCESS;
}

void ConnectionManager::Shutdown()
{
    if (!Initialized.load())
    {
        return;
    }
    
    DisconnectAll();
    
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    Connections.clear();
    
    Initialized.store(false);
}

bool ConnectionManager::IsInitialized() const
{
    return Initialized.load();
}

ConnectionId ConnectionManager::CreateConnection(const NetworkAddress& Address, ProtocolType Protocol)
{
    if (!Initialized.load())
    {
        return InvalidConnectionId;
    }
    
    ConnectionId Id = GenerateConnectionId();
    
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto Info = std::make_unique<ConnectionInfo>();
    Info->Id = Id;
    Info->Address = Address;
    Info->Protocol = Protocol;
    Info->State = ConnectionState::DISCONNECTED;
    Info->MaxRetries = 3;  // Default value
    Info->RetryIntervalMs = 1000;  // Default value
    Info->MaxRetryIntervalMs = 30000;  // Default value
    Info->EnableHeartbeat = Config.EnableKeepalive;
    Info->HeartbeatIntervalMs = Config.KeepAliveIntervalMs;
    
    Connections[Id] = std::move(Info);
    
    return Id;
}

NetworkError ConnectionManager::Connect(ConnectionId Id)
{
    if (!Initialized.load())
    {
        return NetworkError::NOT_INITIALIZED;
    }
    
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return NetworkError::CONNECTION_NOT_FOUND;
    }
    
    ConnectionInfo* Info = It->second.get();
    
    if (Info->State == ConnectionState::CONNECTED || Info->State == ConnectionState::CONNECTING)
    {
        return NetworkError::SUCCESS;  // Already connected or connecting
    }
    
    // For now, just update the state to connected
    // In a real implementation, this would create and connect a socket
    UpdateConnectionStateInternal(Id, ConnectionState::CONNECTING);
    
    // Simulate connection success
    UpdateConnectionStateInternal(Id, ConnectionState::CONNECTED);
    Info->ConnectTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    Info->LastActivityTime = Info->ConnectTime;
    Info->RetryCount = 0;
    
    return NetworkError::SUCCESS;
}

void ConnectionManager::Disconnect(ConnectionId Id)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return;
    }
    
    ConnectionInfo* Info = It->second.get();
    
    if (Info->Socket)
    {
        // In a real implementation, this would close the socket
        Info->Socket.reset();
    }
    
    // Use internal method to avoid deadlock while still triggering callbacks
    UpdateConnectionStateInternal(Id, ConnectionState::DISCONNECTED);
}

void ConnectionManager::DisconnectAll()
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    for (auto& [Id, Info] : Connections)
    {
        if (Info->Socket)
        {
            Info->Socket.reset();
        }
        // Use internal method to avoid deadlock while still triggering callbacks
        UpdateConnectionStateInternal(Id, ConnectionState::DISCONNECTED);
    }
    
    // Clear reconnection queue
    std::lock_guard<std::mutex> QueueLock(ReconnectionQueueMutex);
    std::queue<ConnectionId> Empty;
    ReconnectionQueue.swap(Empty);
}

ConnectionInfo* ConnectionManager::GetConnection(ConnectionId Id)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    return (It != Connections.end()) ? It->second.get() : nullptr;
}

const ConnectionInfo* ConnectionManager::GetConnection(ConnectionId Id) const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    return (It != Connections.end()) ? It->second.get() : nullptr;
}

std::vector<ConnectionId> ConnectionManager::GetActiveConnections() const
{
    return GetConnectionsByState(ConnectionState::CONNECTED);
}

std::vector<ConnectionId> ConnectionManager::GetConnectionsByState(ConnectionState State) const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    std::vector<ConnectionId> Result;
    for (const auto& [Id, Info] : Connections)
    {
        if (Info->State == State)
        {
            Result.push_back(Id);
        }
    }
    
    return Result;
}

size_t ConnectionManager::GetConnectionCount() const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    return Connections.size();
}

NetworkError ConnectionManager::SendData(ConnectionId Id, const char* Data, size_t Size)
{
    if (!Initialized.load())
    {
        return NetworkError::NOT_INITIALIZED;
    }
    
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return NetworkError::CONNECTION_NOT_FOUND;
    }
    
    ConnectionInfo* Info = It->second.get();
    
    if (Info->State != ConnectionState::CONNECTED)
    {
        return NetworkError::CONNECTION_NOT_FOUND;
    }
    
    // Simulate successful send
    Info->TotalBytesSent += Size;
    Info->LastActivityTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return NetworkError::SUCCESS;
}

NetworkError ConnectionManager::BroadcastData(const char* Data, size_t Size, ProtocolType Protocol)
{
    if (!Initialized.load())
    {
        return NetworkError::NOT_INITIALIZED;
    }
    
    std::vector<ConnectionId> ActiveConnections = GetActiveConnections();
    return SendToConnections(ActiveConnections, Data, Size);
}

NetworkError ConnectionManager::SendToConnections(const std::vector<ConnectionId>& ConnectionIds, const char* Data, size_t Size)
{
    NetworkError LastError = NetworkError::SUCCESS;
    
    for (ConnectionId Id : ConnectionIds)
    {
        NetworkError Result = SendData(Id, Data, Size);
        if (Result != NetworkError::SUCCESS)
        {
            LastError = Result;
        }
    }
    
    return LastError;
}

void ConnectionManager::EnableAutoReconnect(ConnectionId Id, bool Enable)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It != Connections.end())
    {
        It->second->MaxRetries = Enable ? 3 : 0;  // Default max retries
    }
}

void ConnectionManager::SetReconnectionSettings(ConnectionId Id, uint32_t MaxRetries, uint32_t RetryIntervalMs)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It != Connections.end())
    {
        It->second->MaxRetries = MaxRetries;
        It->second->RetryIntervalMs = RetryIntervalMs;
    }
}

void ConnectionManager::TriggerReconnection(ConnectionId Id)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It != Connections.end())
    {
        ConnectionInfo* Info = It->second.get();
        
        if (Info->State == ConnectionState::CONNECTED)
        {
            Disconnect(Id);
        }
        
        if (ShouldReconnect(*Info))
        {
            std::lock_guard<std::mutex> QueueLock(ReconnectionQueueMutex);
            ReconnectionQueue.push(Id);
        }
    }
}

void ConnectionManager::EnableHeartbeat(ConnectionId Id, bool Enable)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It != Connections.end())
    {
        It->second->EnableHeartbeat = Enable;
    }
}

void ConnectionManager::SetHeartbeatSettings(ConnectionId Id, uint32_t IntervalMs, uint32_t MaxMissed)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It != Connections.end())
    {
        It->second->HeartbeatIntervalMs = IntervalMs;
        It->second->MaxMissedHeartbeats = MaxMissed;
    }
}

void ConnectionManager::ProcessEvents()
{
    if (!Initialized.load())
    {
        return;
    }
    
    ProcessReconnections();
    ProcessHeartbeats();
    CleanupFailedConnections();
}

void ConnectionManager::Update(float DeltaTimeMs)
{
    ProcessEvents();
}

ConnectionStats ConnectionManager::GetConnectionStats(ConnectionId Id) const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return ConnectionStats{};
    }
    
    const ConnectionInfo* Info = It->second.get();
    
    ConnectionStats Stats;
    Stats.BytesSent = Info->TotalBytesSent;
    Stats.BytesReceived = Info->TotalBytesReceived;
    
    return Stats;
}

ConnectionStats ConnectionManager::GetTotalStats() const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    ConnectionStats TotalStats;
    
    for (const auto& [Id, Info] : Connections)
    {
        TotalStats.BytesSent += Info->TotalBytesSent;
        TotalStats.BytesReceived += Info->TotalBytesReceived;
    }
    
    return TotalStats;
}

void ConnectionManager::ResetStats()
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    for (auto& [Id, Info] : Connections)
    {
        Info->TotalBytesSent = 0;
        Info->TotalBytesReceived = 0;
        Info->FailedAttempts = 0;
    }
}

void ConnectionManager::SetConfig(const NetworkConfig& NetworkConfig)
{
    Config = NetworkConfig;
}

NetworkConfig ConnectionManager::GetConfig() const
{
    return Config;
}

void ConnectionManager::SetOnConnectionStateChanged(std::function<void(ConnectionId, ConnectionState, ConnectionState)> Handler)
{
    OnConnectionStateChanged = Handler;
}

void ConnectionManager::SetOnDataReceived(std::function<void(ConnectionId, const char*, size_t)> Handler)
{
    OnDataReceived = Handler;
}

void ConnectionManager::SetOnConnectionError(std::function<void(ConnectionId, NetworkError)> Handler)
{
    OnConnectionError = Handler;
}

ConnectionId ConnectionManager::GenerateConnectionId()
{
    return NextConnectionId.fetch_add(1);
}

void ConnectionManager::UpdateConnectionState(ConnectionId Id, ConnectionState NewState)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return;
    }
    
    ConnectionInfo* Info = It->second.get();
    ConnectionState OldState = Info->State;
    Info->State = NewState;
    
    // Notify event handler (without additional lock to avoid deadlock)
    if (OnConnectionStateChanged)
    {
        OnConnectionStateChanged(Id, OldState, NewState);
    }
}

// Internal method for updating state when already holding the lock
void ConnectionManager::UpdateConnectionStateInternal(ConnectionId Id, ConnectionState NewState)
{
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return;
    }
    
    ConnectionInfo* Info = It->second.get();
    ConnectionState OldState = Info->State;
    Info->State = NewState;
    
    // Notify event handler
    if (OnConnectionStateChanged)
    {
        OnConnectionStateChanged(Id, OldState, NewState);
    }
}

void ConnectionManager::ProcessReconnections()
{
    std::lock_guard<std::mutex> QueueLock(ReconnectionQueueMutex);
    
    // Process only a limited number of reconnections per call to avoid blocking
    int ProcessedCount = 0;
    const int MaxProcessPerCall = 10;
    
    while (!ReconnectionQueue.empty() && ProcessedCount < MaxProcessPerCall)
    {
        ConnectionId Id = ReconnectionQueue.front();
        ReconnectionQueue.pop();
        ProcessedCount++;
        
        std::lock_guard<std::mutex> Lock(ConnectionsMutex);
        
        auto It = Connections.find(Id);
        if (It == Connections.end())
        {
            continue;
        }
        
        ConnectionInfo* Info = It->second.get();
        
        if (Info->State == ConnectionState::CONNECTED)
        {
            continue;  // Already connected
        }
        
        if (Info->RetryCount >= Info->MaxRetries)
        {
            UpdateConnectionState(Id, ConnectionState::ERROR_STATE);
            continue;
        }
        
        UpdateConnectionState(Id, ConnectionState::CONNECTING);
        
        // Simulate reconnection attempt
        NetworkError Result = Connect(Id);
        if (Result != NetworkError::SUCCESS)
        {
            Info->RetryCount++;
            if (Info->RetryCount < Info->MaxRetries)
            {
                // Don't immediately retry - let it be processed in the next call
                // This prevents infinite loops
            }
            else
            {
                UpdateConnectionState(Id, ConnectionState::ERROR_STATE);
            }
        }
    }
}

void ConnectionManager::ProcessHeartbeats()
{
    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    for (auto& [Id, Info] : Connections)
    {
        if (Info->State != ConnectionState::CONNECTED || !Info->EnableHeartbeat)
        {
            continue;
        }
        
        if (Now - Info->LastHeartbeatTime >= Info->HeartbeatIntervalMs)
        {
            // Simulate heartbeat send
            Info->LastHeartbeatTime = Now;
            Info->MissedHeartbeats = 0;
        }
    }
}

void ConnectionManager::CleanupFailedConnections()
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.begin();
    while (It != Connections.end())
    {
        if (It->second->State == ConnectionState::ERROR_STATE)
        {
            It = Connections.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

void ConnectionManager::HandleConnectionError(ConnectionId Id, NetworkError Error)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    
    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return;
    }
    
    ConnectionInfo* Info = It->second.get();
    
    if (Info->Socket)
    {
        Info->Socket.reset();
    }
    
    UpdateConnectionState(Id, ConnectionState::DISCONNECTED);
    
    // Queue for reconnection if enabled
    if (ShouldReconnect(*Info))
    {
        std::lock_guard<std::mutex> QueueLock(ReconnectionQueueMutex);
        ReconnectionQueue.push(Id);
    }
    
    // Notify error handler
    if (OnConnectionError)
    {
        OnConnectionError(Id, Error);
    }
}

bool ConnectionManager::ShouldReconnect(const ConnectionInfo& Info) const
{
    return Info.RetryCount < Info.MaxRetries && Info.MaxRetries > 0;
}

uint32_t ConnectionManager::CalculateRetryInterval(const ConnectionInfo& Info) const
{
    // Exponential backoff with jitter
    uint32_t BaseInterval = Info.RetryIntervalMs;
    uint32_t MaxInterval = Info.MaxRetryIntervalMs;
    
    uint32_t Interval = BaseInterval * (1 << std::min(Info.RetryCount, 10U));
    Interval = std::min(Interval, MaxInterval);
    
    // Add jitter (±25%)
    uint32_t Jitter = Interval / 4;
    Interval += (rand() % (2 * Jitter)) - Jitter;
    
    return Interval;
}

}  // namespace Helianthus::Network