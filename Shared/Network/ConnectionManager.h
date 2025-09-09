#pragma once

#include "NetworkTypes.h"
#include "INetworkSocket.h"

#include <unordered_map>
#include <queue>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace Helianthus::Network
{

// Use the existing ConnectionState from NetworkTypes.h

/**
 * @brief Connection information structure
 */
struct ConnectionInfo
{
    ConnectionId Id = InvalidConnectionId;
    NetworkAddress Address;
    ProtocolType Protocol = ProtocolType::TCP;
    ConnectionState State = ConnectionState::DISCONNECTED;
    NetworkSocketPtr Socket;
    
    // Reconnection settings
    uint32_t MaxRetries = 3;
    uint32_t RetryCount = 0;
    uint32_t RetryIntervalMs = 1000;
    uint32_t MaxRetryIntervalMs = 30000;
    
    // Statistics
    uint64_t ConnectTime = 0;
    uint64_t LastActivityTime = 0;
    uint64_t TotalBytesSent = 0;
    uint64_t TotalBytesReceived = 0;
    uint32_t FailedAttempts = 0;
    
    // Health check
    bool EnableHeartbeat = true;
    uint32_t HeartbeatIntervalMs = 30000;
    uint64_t LastHeartbeatTime = 0;
    uint32_t MissedHeartbeats = 0;
    uint32_t MaxMissedHeartbeats = 3;
};

/**
 * @brief Connection manager for handling multiple network connections
 */
class ConnectionManager
{
public:
    ConnectionManager();
    ~ConnectionManager();

    // Lifecycle
    NetworkError Initialize(const NetworkConfig& Config);
    void Shutdown();
    bool IsInitialized() const;

    // Connection management
    ConnectionId CreateConnection(const NetworkAddress& Address, ProtocolType Protocol);
    NetworkError Connect(ConnectionId Id);
    void Disconnect(ConnectionId Id);
    void DisconnectAll();

    // Connection information
    ConnectionInfo* GetConnection(ConnectionId Id);
    const ConnectionInfo* GetConnection(ConnectionId Id) const;
    std::vector<ConnectionId> GetActiveConnections() const;
    std::vector<ConnectionId> GetConnectionsByState(ConnectionState State) const;
    size_t GetConnectionCount() const;

    // Data operations
    NetworkError SendData(ConnectionId Id, const char* Data, size_t Size);
    NetworkError BroadcastData(const char* Data, size_t Size, ProtocolType Protocol = ProtocolType::TCP);
    NetworkError SendToConnections(const std::vector<ConnectionId>& ConnectionIds, const char* Data, size_t Size);

    // Reconnection management
    void EnableAutoReconnect(ConnectionId Id, bool Enable);
    void SetReconnectionSettings(ConnectionId Id, uint32_t MaxRetries, uint32_t RetryIntervalMs);
    void TriggerReconnection(ConnectionId Id);

    // Heartbeat management
    void EnableHeartbeat(ConnectionId Id, bool Enable);
    void SetHeartbeatSettings(ConnectionId Id, uint32_t IntervalMs, uint32_t MaxMissed);

    // Event processing
    void ProcessEvents();
    void Update(float DeltaTimeMs);

    // Statistics
    ConnectionStats GetConnectionStats(ConnectionId Id) const;
    ConnectionStats GetTotalStats() const;
    void ResetStats();

    // Configuration
    void SetConfig(const NetworkConfig& Config);
    NetworkConfig GetConfig() const;

    // Event handlers
    void SetOnConnectionStateChanged(std::function<void(ConnectionId, ConnectionState, ConnectionState)> Handler);
    void SetOnDataReceived(std::function<void(ConnectionId, const char*, size_t)> Handler);
    void SetOnConnectionError(std::function<void(ConnectionId, NetworkError)> Handler);

private:
    // Internal state
    std::atomic<bool> Initialized{false};
    NetworkConfig Config;
    
    // Connection storage
    std::unordered_map<ConnectionId, std::unique_ptr<ConnectionInfo>> Connections;
    mutable std::mutex ConnectionsMutex;
    
    // Connection ID generation
    std::atomic<ConnectionId> NextConnectionId{1};
    
    // Reconnection queue
    std::queue<ConnectionId> ReconnectionQueue;
    std::mutex ReconnectionQueueMutex;
    
    // Event handlers
    std::function<void(ConnectionId, ConnectionState, ConnectionState)> OnConnectionStateChanged;
    std::function<void(ConnectionId, const char*, size_t)> OnDataReceived;
    std::function<void(ConnectionId, NetworkError)> OnConnectionError;

    // Internal methods
    ConnectionId GenerateConnectionId();
    void UpdateConnectionState(ConnectionId Id, ConnectionState NewState);
    void UpdateConnectionStateInternal(ConnectionId Id, ConnectionState NewState);
    void ProcessReconnections();
    void ProcessHeartbeats();
    void CleanupFailedConnections();
    void HandleConnectionError(ConnectionId Id, NetworkError Error);
    bool ShouldReconnect(const ConnectionInfo& Info) const;
    uint32_t CalculateRetryInterval(const ConnectionInfo& Info) const;
    
    // Socket event handling
    void OnSocketConnected(ConnectionId Id);
    void OnSocketDisconnected(ConnectionId Id, NetworkError Error);
    void OnSocketDataReceived(ConnectionId Id, const char* Data, size_t Size);
    void OnSocketError(ConnectionId Id, NetworkError Error);
};

}  // namespace Helianthus::Network
