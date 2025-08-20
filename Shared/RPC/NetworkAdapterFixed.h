#pragma once

#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

namespace Helianthus::RPC
{
/**
 * @brief Improved NetworkAdapter that properly handles message routing for RPC
 *
 * This implementation provides a working message routing system that bridges
 * the gap between RPC requirements and simplified networking for demo purposes.
 */
class NetworkAdapterFixed
{
public:
    NetworkAdapterFixed();
    ~NetworkAdapterFixed();

    // Basic networking API
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
    // Message structure for internal communication
    struct InternalMessage
    {
        Network::ConnectionId ConnectionId;
        std::vector<char> Data;
        std::chrono::steady_clock::time_point Timestamp;
    };

    // Connection information
    struct ConnectionInfo
    {
        Network::NetworkAddress Address;
        bool IsServerConnection = false;
        std::chrono::steady_clock::time_point LastActivity;
    };

    // Internal state
    bool IsInitialized = false;
    bool IsServerRunning = false;
    Network::NetworkAddress ServerAddress;
    Network::NetworkConfig Config;

    // Connection tracking
    std::mutex ConnectionMutex;
    std::unordered_map<Network::ConnectionId, ConnectionInfo> Connections;
    std::atomic<Network::ConnectionId> NextConnectionId{1};

    // Message queues
    std::mutex IncomingMutex;
    std::condition_variable IncomingCondition;
    std::queue<InternalMessage> IncomingMessages;

    std::mutex OutgoingMutex;
    std::queue<InternalMessage> OutgoingMessages;

    // Worker threads
    std::thread MessageProcessingThread;
    std::atomic<bool> ShouldStop{false};

    // Event callbacks
    std::function<void(Network::ConnectionId)> ClientConnectedCallback;
    std::function<void(Network::ConnectionId, Network::NetworkError)> ClientDisconnectedCallback;
    std::function<void(Network::ConnectionId, const char*, size_t)> DataReceivedCallback;

    // Internal methods
    void StartMessageProcessingThread();
    void StopMessageProcessingThread();
    void MessageProcessingLoop();
    void ProcessIncomingMessages();
    void ProcessOutgoingMessages();
    Network::ConnectionId GenerateConnectionId();
    void SimulateNetworkDelay();
};

}  // namespace Helianthus::RPC