#pragma once

#include <unordered_map>
#include <vector>

#include "INetworkSocket.h"
#include "NetworkTypes.h"

namespace Helianthus::Network
{
/**
 * @brief Abstract interface for network connection management
 *
 * The NetworkManager handles multiple connections, connection pooling,
 * and high-level network operations for the game server.
 */
class INetworkManager
{
public:
    virtual ~INetworkManager() = default;

    // Initialization and cleanup
    virtual NetworkError Initialize(const NetworkConfig& Config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Server operations
    virtual NetworkError StartServer(const NetworkAddress& Address, ProtocolType Protocol) = 0;
    virtual void StopServer() = 0;
    virtual bool IsServerRunning() const = 0;

    // Client connection management
    virtual ConnectionId ConnectToServer(const NetworkAddress& Address, ProtocolType Protocol) = 0;
    virtual void DisconnectClient(ConnectionId ClientId) = 0;
    virtual void DisconnectAllClients() = 0;

    // Socket management
    virtual NetworkSocketPtr CreateSocket(ProtocolType Protocol) = 0;
    virtual NetworkSocketPtr GetSocket(ConnectionId ConnectionId) = 0;
    virtual void RemoveSocket(ConnectionId ConnectionId) = 0;
    virtual size_t GetActiveConnectionCount() const = 0;
    virtual std::vector<ConnectionId> GetActiveConnections() const = 0;

    // Data broadcasting
    virtual NetworkError
    BroadcastData(const char* Data, size_t Size, ProtocolType Protocol = ProtocolType::TCP) = 0;
    virtual NetworkError SendToClient(ConnectionId ClientId, const char* Data, size_t Size) = 0;
    virtual NetworkError
    SendToClients(const std::vector<ConnectionId>& ClientIds, const char* Data, size_t Size) = 0;

    // Connection pool management
    virtual void SetMaxConnections(uint32_t MaxConnections) = 0;
    virtual uint32_t GetMaxConnections() const = 0;
    virtual void SetConnectionTimeout(uint32_t TimeoutMs) = 0;
    virtual uint32_t GetConnectionTimeout() const = 0;

    // Event processing
    virtual void ProcessNetworkEvents() = 0;
    virtual void Update(float DeltaTimeMs) = 0;

    // Statistics and monitoring
    virtual void
    GetNetworkStats(std::unordered_map<ConnectionId, ConnectionStats>& Stats) const = 0;
    virtual ConnectionStats GetTotalStats() const = 0;
    virtual void ResetAllStats() = 0;

    // Configuration
    virtual void UpdateConfig(const NetworkConfig& Config) = 0;
    virtual NetworkConfig GetCurrentConfig() const = 0;

    // Callback registration for manager-level events
    virtual void SetOnClientConnectedCallback(std::function<void(ConnectionId)> Callback) = 0;
    virtual void
    SetOnClientDisconnectedCallback(std::function<void(ConnectionId, NetworkError)> Callback) = 0;
    virtual void
    SetOnDataReceivedCallback(std::function<void(ConnectionId, const char*, size_t)> Callback) = 0;
    virtual void
    SetOnServerErrorCallback(std::function<void(NetworkError, const std::string&)> Callback) = 0;

    // Advanced features
    virtual void SetConnectionLimit(uint32_t Limit) = 0;
    virtual void SetBandwidthLimit(uint64_t BytesPerSecond) = 0;
    virtual void EnableConnectionThrottling(bool Enable) = 0;
    virtual void BlacklistAddress(const std::string& IpAddress) = 0;
    virtual void RemoveFromBlacklist(const std::string& IpAddress) = 0;
    virtual bool IsAddressBlacklisted(const std::string& IpAddress) const = 0;
};

}  // namespace Helianthus::Network
