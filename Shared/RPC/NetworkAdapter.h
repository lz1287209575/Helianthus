#pragma once

#include "Shared/Network/NetworkManager.h"
#include "Shared/Network/NetworkTypes.h"

#include <functional>

namespace Helianthus::RPC
{
/**
 * @brief Adapter class to bridge RPC requirements with the current NetworkManager API
 *
 * This is a temporary solution to resolve API mismatches between the RPC system
 * and the existing NetworkManager implementation.
 */
class NetworkAdapter
{
public:
    NetworkAdapter();
    ~NetworkAdapter();

    // RPC-specific networking API
    Network::NetworkError Initialize(const Network::NetworkConfig& Config);
    void Shutdown();

    // Server operations
    Network::NetworkError StartServer(const Network::NetworkAddress& Address,
                                      Network::ProtocolType Protocol);
    void StopServer();

    // Client operations
    Network::ConnectionId ConnectToServer(const Network::NetworkAddress& Address,
                                          Network::ProtocolType Protocol);
    void DisconnectClient(Network::ConnectionId ClientId);

    // Data operations
    Network::NetworkError
    SendToClient(Network::ConnectionId ClientId, const char* Data, size_t Size);

    // Event processing
    void ProcessNetworkEvents();

    // Event callbacks
    void SetOnClientConnectedCallback(std::function<void(Network::ConnectionId)> Callback);
    void SetOnClientDisconnectedCallback(
        std::function<void(Network::ConnectionId, Network::NetworkError)> Callback);
    void SetOnDataReceivedCallback(
        std::function<void(Network::ConnectionId, const char*, size_t)> Callback);

private:
    std::shared_ptr<Network::NetworkManager> NetworkManager;
    std::function<void(Network::ConnectionId)> ClientConnectedCallback;
    std::function<void(Network::ConnectionId, Network::NetworkError)> ClientDisconnectedCallback;
    std::function<void(Network::ConnectionId, const char*, size_t)> DataReceivedCallback;

    bool IsInitialized = false;
    bool IsServerRunning = false;
    Network::NetworkAddress ServerAddress;

    // Connection tracking for adaptation
    std::unordered_map<Network::ConnectionId, Network::NetworkAddress> ConnectionAddresses;
    std::mutex ConnectionMutex;
};

}  // namespace Helianthus::RPC