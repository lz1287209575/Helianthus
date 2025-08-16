#pragma once

#include "MessageRouter.h"
#include "Shared/Network/NetworkTypes.h"
#include <functional>

namespace Helianthus::RPC
{
    /**
     * @brief Simplified NetworkAdapter using global MessageRouter
     * 
     * This implementation uses a global message router to properly
     * simulate client-server communication for RPC demo purposes.
     */
    class NetworkAdapterSimple
    {
    public:
        NetworkAdapterSimple();
        ~NetworkAdapterSimple();

        // Basic networking API
        Network::NetworkError Initialize(const Network::NetworkConfig& Config);
        void Shutdown();

        // Server operations
        Network::NetworkError StartServer(const Network::NetworkAddress& Address, Network::ProtocolType Protocol);
        void StopServer();

        // Client operations
        Network::ConnectionId ConnectToServer(const Network::NetworkAddress& Address, Network::ProtocolType Protocol);
        void DisconnectClient(Network::ConnectionId ClientId);

        // Data operations
        Network::NetworkError SendToClient(Network::ConnectionId ClientId, const uint8_t* Data, size_t Size);

        // Event processing
        void ProcessNetworkEvents();

        // Event callbacks
        void SetOnClientConnectedCallback(std::function<void(Network::ConnectionId)> Callback);
        void SetOnClientDisconnectedCallback(std::function<void(Network::ConnectionId, Network::NetworkError)> Callback);
        void SetOnDataReceivedCallback(std::function<void(Network::ConnectionId, const uint8_t*, size_t)> Callback);

    private:
        // State
        bool IsInitialized = false;
        bool IsServerRunning = false;
        Network::NetworkAddress ServerAddress;
        Network::ConnectionId ClientConnectionId = Network::InvalidConnectionId;
        
        // Event callbacks
        std::function<void(Network::ConnectionId)> ClientConnectedCallback;
        std::function<void(Network::ConnectionId, Network::NetworkError)> ClientDisconnectedCallback;
        std::function<void(Network::ConnectionId, const uint8_t*, size_t)> DataReceivedCallback;
        
        // Message routing callbacks
        void HandleIncomingMessage(Network::ConnectionId ConnId, const uint8_t* Data, size_t Size);
    };

} // namespace Helianthus::RPC